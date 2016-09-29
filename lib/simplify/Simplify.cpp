#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "Common.h"
#include "Simplify.h"

using namespace llvm;
using namespace std;
using namespace pasha;

bool Simplify::doInitialization(Module &M) { return false; }

bool Simplify::doFinalization(Module &M) { return false; }

bool Simplify::runOnModule(Module &M) {
    for (auto &F : M) {
        if (F.getName() == FunctionName) {
            common::lowerSwitch(F);
            common::breakCritEdges(F);

            auto updatePhis = [](BasicBlock *Tgt, BasicBlock *New) {
                for (auto &I : *Tgt) {
                    if (auto *Phi = dyn_cast<PHINode>(&I)) {
                        Phi->setIncomingBlock(Phi->getBasicBlockIndex(Tgt),
                                              New);
                    }
                }
            };
            auto insertLatch = [&updatePhis](BasicBlock *BB) {
                auto *Latch = BasicBlock::Create(BB->getContext(),
                                                 BB->getName() + ".latch",
                                                 BB->getParent());
                auto *T = BB->getTerminator();
                T->replaceUsesOfWith(BB, Latch);
                updatePhis(BB, Latch);
                BranchInst::Create(BB, Latch);
            };

            for (auto &F : M) {
                for (auto &BB : F) {
                    if (common::isSelfLoop(&BB)) {
                        insertLatch(&BB);
                    }
                }
            }
        }
    }
    return true;
}

char Simplify::ID = 0;
// static RegisterPass<Simplify>
// X("", "Simplify CFG -- lower switch and break crit edges");
