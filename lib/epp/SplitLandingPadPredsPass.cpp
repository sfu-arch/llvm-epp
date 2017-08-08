#include "SplitLandingPadPredsPass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace epp;

namespace {

void SplitLandingPadPreds(Function &F) {
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (InvokeInst *II = dyn_cast<InvokeInst>(&I)) {
                BasicBlock *LPad = II->getUnwindDest();

                // Check if BB->LPad edge is a critical edge.
                if (BB.getSingleSuccessor() || LPad->getUniquePredecessor())
                    continue;

                SmallVector<BasicBlock *, 2> NewBBs;
                SplitLandingPadPredecessors(LPad, &BB, ".1", ".2", NewBBs);
            }
        }
    }
}
}

bool SplitLandingPadPredsPass::runOnModule(Module &M) {
    for (auto &F : M) {
        if (F.isDeclaration())
            continue;
        SplitLandingPadPreds(F);
    }

    return true;
}

char SplitLandingPadPredsPass::ID = 0;
