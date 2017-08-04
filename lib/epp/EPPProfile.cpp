#define DEBUG_TYPE "epp_profile"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "llvm/Support/FileSystem.h"

#include "AltCFG.h"
#include "EPPEncode.h"
#include "EPPProfile.h"
#include <cassert>
#include <tuple>
#include <unordered_map>

using namespace llvm;
using namespace epp;
using namespace std;

extern cl::opt<string> profileOutputFilename;

bool EPPProfile::doInitialization(Module &M) {
    uint32_t Id = 0;
    for (auto &F : M) {
        FunctionIds[&F] = Id++;
    }

    return false;
}

bool EPPProfile::doFinalization(Module &M) { return false; }

static void saveModule(Module &m, StringRef filename) {
    error_code EC;
    raw_fd_ostream out(filename.data(), EC, sys::fs::F_None);

    if (EC) {
        report_fatal_error("error saving llvm module to '" + filename +
                           "': \n" + EC.message());
    }
    WriteBitcodeToFile(&m, out);
}

void EPPProfile::addCtorsAndDtors(Module &Mod) {
    auto &Ctx = Mod.getContext();
    auto *voidTy    = Type::getVoidTy(Ctx);
    auto *int32Ty   = Type::getInt32Ty(Ctx);
    auto *int8PtrTy = Type::getInt8PtrTy(Ctx, 0);
    uint32_t NumberOfFunctions = FunctionIds.size();

    auto *EPPInit = cast<Function>(
        Mod.getOrInsertFunction("__epp_init", voidTy, int32Ty, nullptr));
    auto *EPPSave = cast<Function>(
        Mod.getOrInsertFunction("__epp_save", voidTy, int8PtrTy, nullptr));

    // Add Global Constructor for initializing path profiling
    auto *EPPInitCtor = cast<Function>(
        Mod.getOrInsertFunction("__epp_ctor", voidTy, nullptr));
    auto *CtorBB = BasicBlock::Create(Ctx, "entry", EPPInitCtor);
    auto *Arg    = ConstantInt::get(int32Ty, NumberOfFunctions, false);
    CallInst::Create(EPPInit, {Arg}, "", CtorBB);
    ReturnInst::Create(Ctx, CtorBB);
    appendToGlobalCtors(Mod, EPPInitCtor, 0);

    auto *Number = ConstantInt::get(int32Ty, NumberOfFunctions, false);
    new GlobalVariable(Mod, Number->getType(), false,
                       GlobalValue::ExternalLinkage, Number,
                       "__epp_numberOfFunctions");

    // Add global destructor to dump out results
    auto *EPPSaveDtor = cast<Function>(
        Mod.getOrInsertFunction("__epp_dtor", voidTy, nullptr));
    auto *DtorBB = BasicBlock::Create(Ctx, "entry", EPPSaveDtor);
    IRBuilder<> Builder(DtorBB);
    Builder.CreateCall(EPPSave, {Builder.CreateGlobalStringPtr(
                                    profileOutputFilename.getValue())});
    Builder.CreateRet(nullptr);

    appendToGlobalDtors(Mod, cast<Function>(EPPSaveDtor), 0);
}

bool EPPProfile::runOnModule(Module &Mod) {
    DEBUG(errs() << "Running Profile\n");

    errs() << "# Instrumented Functions\n";

    for (auto &F : Mod) {
        if (!F.isDeclaration()) {
            errs() << "- name: " << F.getName() << "\n";
            auto &Enc = getAnalysis<EPPEncode>(F);
            errs() << "  num_paths: " << Enc.numPaths[&F.getEntryBlock()]
                   << "\n";
            instrument(F, Enc);
        }
    }
    
    addCtorsAndDtors(Mod);

    return true;
}

static SmallVector<BasicBlock *, 1> getFunctionExitBlocks(Function &F) {
    SmallVector<BasicBlock *, 1> R;
    for (auto &BB : F) {
        if (BB.getTerminator()->getNumSuccessors() == 0) {
            R.push_back(&BB);
        }
    }
    return R;
}

static void insertInc(Instruction *addPos, APInt Increment, AllocaInst *Ctr) {
    if (Increment.ne(APInt(128, 0, true))) {
        (errs() << "Inserting Increment " << Increment << " "
                     << addPos->getParent()->getName() << "\n");
        auto *LI = new LoadInst(Ctr, "ld.epp.ctr", addPos);

        Constant *CI = nullptr;
        auto I64     = APInt(64, Increment.getLimitedValue(), true);
        CI           = ConstantInt::getIntegerValue(Ctr->getAllocatedType(), I64);

        auto *BI = BinaryOperator::CreateAdd(LI, CI);
        BI->insertAfter(LI);
        (new StoreInst(BI, Ctr))->insertAfter(BI);
    }
}

static BasicBlock* interpose(BasicBlock *Src, BasicBlock *Tgt) {
    auto &Ctx = Src->getContext();
    (errs() << "Split : " << Src->getName() << " " << Tgt->getName()
                 << "\n");

    // Sanity Checks
    auto found = false;
    for (auto S = succ_begin(Src), E = succ_end(Src); S != E; S++)
        if (*S == Tgt)
            found = true;
    assert(found && "Could not find the edge to split");

    auto *F  = Tgt->getParent();
    auto *BB = BasicBlock::Create(Ctx, Src->getName() + ".intp", F);

    auto *T = Src->getTerminator();
    T->replaceUsesOfWith(Tgt, BB);

    BranchInst::Create(Tgt, BB);

    // Hoist all special instructions from the Tgt block
    // to the new block. Rewrite the uses of the old instructions
    // to use the instructions in the new block.

    for (auto &I : vector<BasicBlock::iterator>(
             Tgt->begin(), Tgt->getFirstInsertionPt())) {
        I->moveBefore(BB->getTerminator());
    }

    return BB;
}

static void insertLogPath(BasicBlock *BB, uint64_t FuncId, 
        AllocaInst *Ctr, Constant* Zap) {

    errs() << "Inserting Log: " << BB->getName() << "\n";

    Module *M = BB->getModule();
    auto &Ctx = M->getContext();
    auto *voidTy   = Type::getVoidTy(Ctx);
    auto *CtrTy = Ctr->getAllocatedType();
    auto *FIdArg = ConstantInt::getIntegerValue(CtrTy, APInt(64, FuncId, true));
    Function *logFun2 = cast<Function>(M->getOrInsertFunction("__epp_logPath", voidTy,
                                                    CtrTy, CtrTy, nullptr));
    Instruction *logPos = BB->getTerminator();

    // If the terminator is a unreachable inst, then the instruction
    // prior to it is *most* probably a call instruction which does
    // not return. So modify the logPos to point to the instruction
    // before that one.

    if (isa<UnreachableInst>(logPos)) {
        auto Pos  = BB->getFirstInsertionPt();
        auto Next = next(Pos);
        while (&*Next != BB->getTerminator()) {
            Pos++, Next++;
        }
        logPos = &*Pos;
    }

    auto *LI               = new LoadInst(Ctr, "ld.epp.ctr", logPos);
    vector<Value *> Params = {LI, FIdArg};
    auto *CI               = CallInst::Create(logFun2, Params, "");
    CI->insertAfter(LI);
    (new StoreInst(Zap, Ctr))->insertAfter(CI);
}

void EPPProfile::instrument(Function &F, EPPEncode &Enc) {
    Module *M      = F.getParent();
    auto &Ctx      = M->getContext();

    uint64_t FuncId = FunctionIds[&F];

    // 1. Lookup the Function to Function ID mapping here
    // 2. Create a constant int for the id
    // 3. Pass the id in the logpath2 function call

    Type *CtrTy = Type::getInt64Ty(Ctx);
    Constant *Zap   = ConstantInt::getIntegerValue(CtrTy, APInt(64, 0, true));
    auto *Ctr = new AllocaInst(CtrTy, nullptr, "epp.ctr",
                               &*F.getEntryBlock().getFirstInsertionPt());
    auto *SI = new StoreInst(Zap, Ctr);
    SI->insertAfter(Ctr);

    auto ExitBlocks = getFunctionExitBlocks(F);

    // Get all the non-zero real edges to instrument
    const auto &Wts = Enc.AG.getWeights();

    Enc.AG.printWeights();

    for(auto &W : Wts) {
        auto &Ptr = W.first;
        BasicBlock *Src = Ptr->src, *Tgt = Ptr->tgt;
        BasicBlock *N = interpose(Src, Tgt);
        insertInc(&*N->getFirstInsertionPt(), W.second, Ctr);
    }

    // Get the weights for the segmented edges
    const auto &SegMap = Enc.AG.getSegmentMap();

    for(auto &S : SegMap) {
        auto &Ptr = S.first;
        BasicBlock *Src = Ptr->src, *Tgt = Ptr->tgt;
        BasicBlock *N = interpose(Src, Tgt);

        auto &AExit = S.second.first;
        APInt Pre = Enc.AG.getEdgeWeight(AExit);
        auto &EntryB = S.second.second;
        APInt Post = Enc.AG.getEdgeWeight(EntryB);
       
        insertInc(&*N->getFirstInsertionPt(), Pre, Ctr);
        insertLogPath(N, FuncId, Ctr, Zap);
        insertInc(&*N->getTerminator(), Post, Ctr);
    }


    // Add the logpath function for all function exiting
    // basic blocks.
    for (auto &EB : ExitBlocks) {
        insertLogPath(EB, FuncId, Ctr, Zap);
    }
}

//void EPPProfile::instrument(Function &F, EPPEncode &Enc) {
    //Module *M      = F.getParent();
    //auto &Ctx      = M->getContext();

    //uint64_t FuncId = FunctionIds[&F];

    //// 1. Lookup the Function to Function ID mapping here
    //// 2. Create a constant int for the id
    //// 3. Pass the id in the logpath2 function call

    //Type *CtrTy = Type::getInt64Ty(Ctx);
    //Constant *Zap   = ConstantInt::getIntegerValue(CtrTy, APInt(64, 0, true));
    //auto *Ctr = new AllocaInst(CtrTy, nullptr, "epp.ctr",
                               //&*F.getEntryBlock().getFirstInsertionPt());
    //auto *SI = new StoreInst(Zap, Ctr);
    //SI->insertAfter(Ctr);

    //auto ExitBlocks = getFunctionExitBlocks(F);
    //auto *Entry = &F.getEntryBlock(), *Exit = *ExitBlocks.begin();

    //CFGInstHelper Inst(Enc.ACFG, Entry, Exit);

    //APInt BackVal = APInt(128, 0, true);
//#define _ std::ignore
    //tie(_, BackVal, _, _) = Inst.get({Exit, Entry});
//#undef _

    //DEBUG(errs() << "BackVal : " << BackVal.toString(10, true) << "\n");

    //SmallVector<Edge, 32> FunctionEdges;
    //// For each edge in the function, get the increments
    //// for the edge and stick them in there.
    //for (auto &BB : F) {
        //for (auto SB = succ_begin(&BB), SE = succ_end(&BB); SB != SE; SB++) {
            //FunctionEdges.push_back({&BB, *SB});
        //}
    //}

    //for (auto &E : FunctionEdges) {
        //APInt Val1(128, 0, true), Val2(128, 0, true);
        //bool Exists = false, BackedgeLog = false;
        //tie(Exists, Val1, BackedgeLog, Val2) = Inst.get(E);

        //if (Exists) {
            //auto *Split = interpose(SRC(E), TGT(E));
            //if (BackedgeLog) {
                //insertInc(&*Split->getFirstInsertionPt(), Val1 + BackVal, Ctr);
                //insertLogPath(Split, FuncId, Ctr, Zap);
                //insertInc(Split->getTerminator(), Val2, Ctr);
            //} else {
                //insertInc(&*Split->getFirstInsertionPt(), Val1, Ctr);
            //}
        //}
    //}

    //// Add the logpath function for all function exiting
    //// basic blocks.
    //for (auto &EB : ExitBlocks) {
        //insertLogPath(EB, FuncId, Ctr, Zap);
    //}
//}

char EPPProfile::ID = 0;
