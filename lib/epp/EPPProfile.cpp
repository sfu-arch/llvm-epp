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
#include "Common.h"
#include "EPPEncode.h"
#include "EPPProfile.h"
#include <cassert>
#include <tuple>
#include <unordered_map>

using namespace llvm;
using namespace epp;
using namespace std;

extern cl::list<std::string> FunctionList;
extern bool isTargetFunction(const Function &f,
                             const cl::list<std::string> &FunctionList);

bool EPPProfile::doInitialization(Module &m) {
    assert(FunctionList.size() == 1 &&
           "Only one function can be marked for profiling");
    return false;
}

bool EPPProfile::doFinalization(Module &m) { return false; }

bool EPPProfile::runOnModule(Module &module) {
    DEBUG(errs() << "Running Profile\n");
    auto &Ctx = module.getContext();

    for (auto &func : module) {
        if (isTargetFunction(func, FunctionList)) {
            LI        = &getAnalysis<LoopInfoWrapperPass>(func).getLoopInfo();
            auto &enc = getAnalysis<EPPEncode>(func);
            instrument(func, enc);
        }
    }

    auto *voidTy = Type::getVoidTy(Ctx);

    auto *init =
        module.getOrInsertFunction("PaThPrOfIlInG_init", voidTy, nullptr);
    appendToGlobalCtors(module, llvm::cast<Function>(init), 0);

    auto *printer =
        module.getOrInsertFunction("PaThPrOfIlInG_save", voidTy, nullptr);
    appendToGlobalDtors(module, llvm::cast<Function>(printer), 0);

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
    Module *M    = F.getParent();
    auto &Ctx    = M->getContext();
    auto *voidTy = Type::getVoidTy(Ctx);

#ifndef RT32
    auto *CtrTy = Type::getInt128Ty(Ctx);
    auto *Zap   = ConstantInt::getIntegerValue(CtrTy, APInt(128, 0, true));
#else
    auto *CtrTy = Type::getInt64Ty(Ctx);
    auto *Zap   = ConstantInt::getIntegerValue(CtrTy, APInt(64, 0, true));
#endif

    auto *logFun2 = M->getOrInsertFunction("PaThPrOfIlInG_logPath2", voidTy,
                                           CtrTy, nullptr);

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

    auto InsertLogPath = [&logFun2, &Ctr, &CtrTy, &Zap](BasicBlock *BB) {
        auto logPos = BB->getTerminator();
        auto *LI    = new LoadInst(Ctr, "ld.epp.ctr", logPos);
        auto *CI    = CallInst::Create(logFun2, {LI}, "");
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
                DEBUG(errs() << "Val1 : " << Val1.toString(10, true) << "\n");
                DEBUG(errs() << "Val2 : " << Val2.toString(10, true) << "\n");
                InsertInc(&*Split->getFirstInsertionPt(), Val1 + BackVal);
                InsertLogPath(Split);
                InsertInc(Split->getTerminator(), Val2);
            } else {
                DEBUG(errs() << "Val1 : " << Val1.toString(10, true) << "\n");
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
static RegisterPass<EPPProfile> X("", "PASHA - EPPProfile");
