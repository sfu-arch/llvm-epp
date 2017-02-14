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
#include "llvm/Support/Debug.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

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

bool EPPProfile::runOnModule(Module &module) {
    DEBUG(errs() << "Running Profile\n");
    auto &Ctx = module.getContext();

    errs() << "# Instrumented Functions\n";

    uint32_t NumberOfFunctions = 0;
    for (auto &func : module) {
        if (!func.isDeclaration()) {
            errs() << "- name: " << func.getName() << "\n";
            LI        = &getAnalysis<LoopInfoWrapperPass>(func).getLoopInfo();
            auto &enc = getAnalysis<EPPEncode>(func);
            instrument(func, enc);
            NumberOfFunctions++;
        }
    }

    auto *voidTy    = Type::getVoidTy(Ctx);
    auto *int32Ty   = Type::getInt32Ty(Ctx);
    auto *int8PtrTy = Type::getInt8PtrTy(Ctx, 0);
    auto *int8Ty    = Type::getInt8Ty(Ctx);
    auto *Zero      = ConstantInt::get(int32Ty, 0, false);

    // Add Global Constructor for initializing path profiling
    auto *EPPInitCtor = llvm::cast<Function>(
        module.getOrInsertFunction("__epp_ctor", voidTy, nullptr));
    auto *EPPInit = llvm::cast<Function>(module.getOrInsertFunction(
        "PaThPrOfIlInG_init", voidTy, int32Ty, nullptr));
    auto *CtorBB = BasicBlock::Create(Ctx, "entry", EPPInitCtor);
    auto *Arg    = ConstantInt::get(int32Ty, NumberOfFunctions, false);
    CallInst::Create(EPPInit, {Arg}, "", CtorBB);
    ReturnInst::Create(Ctx, CtorBB);
    appendToGlobalCtors(module, EPPInitCtor, 0);

    // Add global destructor to dump out results
    auto *EPPSaveDtor = llvm::cast<Function>(
        module.getOrInsertFunction("__epp_dtor", voidTy, nullptr));
    auto *EPPSave = llvm::cast<Function>(module.getOrInsertFunction(
        "PaThPrOfIlInG_save", voidTy, int8PtrTy, nullptr));
    auto *DtorBB = BasicBlock::Create(Ctx, "entry", EPPSaveDtor);
    IRBuilder<> Builder(DtorBB);
    Builder.CreateCall(EPPSave, {Builder.CreateGlobalStringPtr(
                                    profileOutputFilename.getValue())});
    Builder.CreateRet(nullptr);

    appendToGlobalDtors(module, llvm::cast<Function>(EPPSaveDtor), 0);

    return true;
}

static SmallVector<BasicBlock *, 1> getFunctionExitBlocks(Function &F) {
    SmallVector<BasicBlock *, 1> R;
    for (auto &BB : F) {
        if (dyn_cast<ReturnInst>(BB.getTerminator())) {
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

#ifndef RT32
    auto *CtrTy = Type::getInt128Ty(Ctx);
    auto *Zap   = ConstantInt::getIntegerValue(CtrTy, APInt(128, 0, true));
#else
    auto *CtrTy = Type::getInt64Ty(Ctx);
    auto *Zap   = ConstantInt::getIntegerValue(CtrTy, APInt(64, 0, true));
#endif

    auto *FIdArg =
        ConstantInt::getIntegerValue(FuncIdTy, APInt(32, FuncId, true));
    auto *logFun2 = M->getOrInsertFunction("PaThPrOfIlInG_logPath2", voidTy,
                                           CtrTy, FuncIdTy, nullptr);

    auto *Ctr = new AllocaInst(CtrTy, nullptr, "epp.ctr",
                               &*F.getEntryBlock().getFirstInsertionPt());

    auto *SI = new StoreInst(Zap, Ctr);
    SI->insertAfter(Ctr);

    auto InsertInc = [&Ctr, &CtrTy](Instruction *addPos, APInt Increment) {
        if (Increment.ne(APInt(128, 0, true))) {
            DEBUG(errs() << "Inserting Increment " << Increment << " "
                         << addPos->getParent()->getName() << "\n");
            // Context Counter
            auto *LI = new LoadInst(Ctr, "ld.epp.ctr", addPos);

#ifndef RT32
            auto *CI = ConstantInt::getIntegerValue(CtrTy, Increment);
#else
            auto I64 = APInt(64, Increment.getLimitedValue(), true);
            auto *CI = ConstantInt::getIntegerValue(CtrTy, I64);
#endif
            auto *BI = BinaryOperator::CreateAdd(LI, CI);
            BI->insertAfter(LI);
            (new StoreInst(BI, Ctr))->insertAfter(BI);
        }
    };

    auto InsertLogPath = [&logFun2, &Ctr, &CtrTy, &Zap,
                          &FIdArg](BasicBlock *BB) {
        auto logPos            = BB->getTerminator();
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

    auto patchPhis = [&blockIndex](BasicBlock *Src, BasicBlock *Tgt,
                                   BasicBlock *New) {
        for (auto &I : *Tgt) {
            if (auto *Phi = dyn_cast<PHINode>(&I)) {
                Phi->setIncomingBlock(blockIndex(Phi, Src), New);
            }
        }
    };

    auto interpose = [&Ctx, &patchPhis](BasicBlock *Src,
                                        BasicBlock *Tgt) -> BasicBlock * {
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
        patchPhis(Src, Tgt, BB);
        auto *T = Src->getTerminator();
        T->replaceUsesOfWith(Tgt, BB);
        BranchInst::Create(Tgt, BB);
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
        bool Exists = false, Log = false;
        tie(Exists, Val1, Log, Val2) = Inst.get(E);

        if (Exists) {
            auto *Split = interpose(SRC(E), TGT(E));
            if (Log) {
                InsertInc(&*Split->getFirstInsertionPt(), Val1 + BackVal);
                InsertLogPath(Split);
                InsertInc(Split->getTerminator(), Val2);
            } else {
                InsertInc(&*Split->getFirstInsertionPt(), Val1);
            }
        }
    }

    // Add the logpath function for all function exiting
    // basic blocks.
    for (auto &EB : ExitBlocks) {
        InsertLogPath(EB);
    }
}

char EPPProfile::ID = 0;
static RegisterPass<EPPProfile> X("", "EPPProfile");
