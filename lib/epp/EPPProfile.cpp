#define DEBUG_TYPE "epp_profile"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/CFG.h"
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
#include "llvm/Bitcode/ReaderWriter.h"

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

bool EPPProfile::runOnModule(Module &module) {
    DEBUG(errs() << "Running Profile\n");
    auto &Ctx = module.getContext();

    errs() << "# Instrumented Functions\n";

    uint32_t NumberOfFunctions = FunctionIds.size();

    for (auto &func : module) {
        if (!func.isDeclaration()) {
            errs() << "- name: " << func.getName() << "\n";
            LI        = &getAnalysis<LoopInfoWrapperPass>(func).getLoopInfo();
            auto &enc = getAnalysis<EPPEncode>(func);
            errs() << "  num_paths: " << enc.numPaths[&func.getEntryBlock()]
                   << "\n";
            instrument(func, enc);
        }
    }

    auto *voidTy    = Type::getVoidTy(Ctx);
    auto *int32Ty   = Type::getInt32Ty(Ctx);
    auto *int8PtrTy = Type::getInt8PtrTy(Ctx, 0);
    auto *int8Ty    = Type::getInt8Ty(Ctx);
    auto *Zero      = ConstantInt::get(int32Ty, 0, false);

    Function *EPPInit = nullptr, *EPPSave = nullptr;

    EPPInit = llvm::cast<Function>(module.getOrInsertFunction(
        "__epp_init", voidTy, int32Ty, nullptr));
    EPPSave = llvm::cast<Function>(module.getOrInsertFunction(
        "__epp_save", voidTy, int8PtrTy, nullptr));

    // Add Global Constructor for initializing path profiling
    auto *EPPInitCtor = llvm::cast<Function>(
        module.getOrInsertFunction("__epp_ctor", voidTy, nullptr));
    auto *CtorBB = BasicBlock::Create(Ctx, "entry", EPPInitCtor);
    auto *Arg    = ConstantInt::get(int32Ty, NumberOfFunctions, false);
    CallInst::Create(EPPInit, {Arg}, "", CtorBB);
    ReturnInst::Create(Ctx, CtorBB);
    appendToGlobalCtors(module, EPPInitCtor, 0);
    
    auto *Number = ConstantInt::get(int32Ty, NumberOfFunctions, false);
    new GlobalVariable(module, Number->getType(), false, GlobalValue::ExternalLinkage,
            Number, "__epp_numberOfFunctions");
    

    // Add global destructor to dump out results
    auto *EPPSaveDtor = llvm::cast<Function>(
        module.getOrInsertFunction("__epp_dtor", voidTy, nullptr));
    auto *DtorBB = BasicBlock::Create(Ctx, "entry", EPPSaveDtor);
    IRBuilder<> Builder(DtorBB);
    Builder.CreateCall(EPPSave, {Builder.CreateGlobalStringPtr(
                                    profileOutputFilename.getValue())});
    Builder.CreateRet(nullptr);

    appendToGlobalDtors(module, llvm::cast<Function>(EPPSaveDtor), 0);

    saveModule(module, "testing.bc");

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

void EPPProfile::instrument(Function &F, EPPEncode &Enc) {
    Module *M      = F.getParent();
    auto &Ctx      = M->getContext();
    auto *voidTy   = Type::getVoidTy(Ctx);
    auto *FuncIdTy = Type::getInt32Ty(Ctx);

    auto FuncId = FunctionIds[&F];

    // 1. Lookup the Function to Function ID mapping here
    // 2. Create a constant int for the id
    // 3. Pass the id in the logpath2 function call

    Type *CtrTy   = nullptr;
    Constant *Zap = nullptr;

    CtrTy = Type::getInt64Ty(Ctx);
    Zap   = ConstantInt::getIntegerValue(CtrTy, APInt(64, 0, true));

    Function *logFun2 = nullptr;

    logFun2 = cast<Function>(M->getOrInsertFunction(
        "__epp_logPath", voidTy, CtrTy, FuncIdTy, nullptr));

    auto *FIdArg =
        ConstantInt::getIntegerValue(FuncIdTy, APInt(32, FuncId, true));

    auto *Ctr = new AllocaInst(CtrTy, nullptr, "epp.ctr",
                               &*F.getEntryBlock().getFirstInsertionPt());

    auto *SI = new StoreInst(Zap, Ctr);
    SI->insertAfter(Ctr);

    auto insertInc = [&Ctr, &CtrTy](Instruction *addPos, APInt Increment) {
        if (Increment.ne(APInt(128, 0, true))) {
            DEBUG(errs() << "Inserting Increment " << Increment << " "
                         << addPos->getParent()->getName() << "\n");
            // Context Counter
            auto *LI = new LoadInst(Ctr, "ld.epp.ctr", addPos);

            Constant *CI = nullptr;
            auto I64 = APInt(64, Increment.getLimitedValue(), true);
            CI       = ConstantInt::getIntegerValue(CtrTy, I64);
            //}

            auto *BI = BinaryOperator::CreateAdd(LI, CI);
            BI->insertAfter(LI);
            (new StoreInst(BI, Ctr))->insertAfter(BI);
        }
    };

    auto insertLogPath = [&logFun2, &Ctr, &CtrTy, &Zap,
                          &FIdArg](BasicBlock *BB) {

        Instruction* logPos            = BB->getTerminator();

        // If the terminator is a unreachable inst, then the instruction
        // prior to it is *most* probably a call instruction which does 
        // not return. So modify the logPos to point to the instruction
        // before that one.
        
        if(isa<UnreachableInst>(logPos)) {
            logPos = &*(BasicBlock::iterator(logPos)--);
        }

        auto *LI               = new LoadInst(Ctr, "ld.epp.ctr", logPos);
        vector<Value *> Params = {LI, FIdArg};
        auto *CI               = CallInst::Create(logFun2, Params, "");
        CI->insertAfter(LI);
        (new StoreInst(Zap, Ctr))->insertAfter(CI);
    };

    auto blockIndex = [](const PHINode *Phi, const BasicBlock *BB) -> uint32_t {
        for (uint32_t I = 0; I < Phi->getNumIncomingValues(); I++) {
            if (Phi->getIncomingBlock(I) == BB)
                return I;
        }
        assert(false && "Unreachable");
    };

    auto interpose = [&Ctx](BasicBlock *Src, BasicBlock *Tgt) -> BasicBlock * {
        DEBUG(errs() << "Split : " << Src->getName() << " " << Tgt->getName()
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
       
        for(auto &I : vector<BasicBlock::iterator>(Tgt->begin(), 
                    Tgt->getFirstInsertionPt())) {
            I->moveBefore(BB->getTerminator());
        }

        return BB;
    };

    auto ExitBlocks = getFunctionExitBlocks(F);
    auto *Entry = &F.getEntryBlock(), *Exit = *ExitBlocks.begin();

    CFGInstHelper Inst(Enc.ACFG, Entry, Exit);

    APInt BackVal = APInt(128, 0, true);
#define _ std::ignore
    tie(_, BackVal, _, _) = Inst.get({Exit, Entry});
#undef _

    DEBUG(errs() << "BackVal : " << BackVal.toString(10, true) << "\n");

    SmallVector<Edge, 32> FunctionEdges;
    // For each edge in the function, get the increments
    // for the edge and stick them in there.
    for (auto &BB : F) {
        for (auto SB = succ_begin(&BB), SE = succ_end(&BB); SB != SE; SB++) {
            FunctionEdges.push_back({&BB, *SB});
        }
    }

    for (auto &E : FunctionEdges) {
        APInt Val1(128, 0, true), Val2(128, 0, true);
        bool Exists = false, BackedgeLog = false;
        tie(Exists, Val1, BackedgeLog, Val2) = Inst.get(E);

        if (Exists) {
            auto *Split = interpose(SRC(E), TGT(E));
            if (BackedgeLog) {
                insertInc(&*Split->getFirstInsertionPt(), Val1 + BackVal);
                insertLogPath(Split);
                insertInc(Split->getTerminator(), Val2);
            } else {
                insertInc(&*Split->getFirstInsertionPt(), Val1);
            }
        }
    }

    // Add the logpath function for all function exiting
    // basic blocks.
    for (auto &EB : ExitBlocks) {
        insertLogPath(EB);
    }

}

char EPPProfile::ID = 0;
