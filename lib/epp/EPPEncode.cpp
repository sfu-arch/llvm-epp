#define DEBUG_TYPE "epp_encode"
#include "Common.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"

#include "EPPEncode.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <set>
#include <stack>
#include <unordered_set>
#include <vector>

using namespace llvm;
using namespace epp;
using namespace std;

bool EPPEncode::doInitialization(Module &m) { return false; }
bool EPPEncode::doFinalization(Module &m) { return false; }

bool EPPEncode::runOnFunction(Function &func) {
    LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    encode(func);
    return false;
}

static bool isFunctionExiting(BasicBlock *BB) {
    if (BB->getTerminator()->getNumSuccessors() == 0)
        return true;

    return false;
}

void EPPEncode::releaseMemory() {
    LI = nullptr;
    numPaths.clear();
    ACFG.clear();
}

void EPPEncode::encode(Function &F) {
    DEBUG(errs() << "Called Encode on " << F.getName() << "\n");

    auto POB   = common::postOrder(F, LI);
    auto Entry = POB.back(), Exit = POB.front();
    auto BackEdges = common::getBackEdges(F);

    // Add real edges
    for (auto &BB : POB) {
        for (auto S = succ_begin(BB), E = succ_end(BB); S != E; S++) {
            if (BackEdges.count(make_pair(BB, *S)) ||
                LI->getLoopFor(BB) != LI->getLoopFor(*S)) {
                DEBUG(errs() << "Adding segmented edge : " << BB->getName()
                             << " " << S->getName() << " " << Entry->getName()
                             << " " << Exit->getName() << "\n");
                ACFG.add(BB, *S, Entry, Exit);
                continue;
            }
            DEBUG(errs() << "Adding Real edge : " << BB->getName() << " "
                         << S->getName() << "\n");
            ACFG.add(BB, *S);
        }
    }

    for (auto &B : POB) {
        APInt pathCount(128, 0, true);

        if (isFunctionExiting(B))
            pathCount = 1;

        for (auto &S : ACFG.succs(B)) {
            ACFG[{B, S}] = pathCount;
            if (numPaths.count(S) == 0)
                numPaths.insert(make_pair(S, APInt(128, 0, true)));

            // This is the only place we need to check for overflow.
            bool Ov   = false;
            pathCount = pathCount.sadd_ov(numPaths[S], Ov);
            if (Ov) {
                report_fatal_error("Integer Overflow");
            }
        }
        numPaths.insert({B, pathCount});
    }

#ifdef RT32
    if (numPaths[Entry].getLimitedValue() == ~0ULL) {
        report_fatal_error(
            "Numpaths greater than 2^64, recompile in 64-bit mode");
    }
// assert(numPaths[Entry].getLimitedValue() < ~0ULL &&
//"Numpaths greater than 2^64, recompile in 64-bit mode");
#endif

    errs() << "NumPaths : " << numPaths[Entry] << "\n";
}

char EPPEncode::ID = 0;
static RegisterPass<EPPEncode> X("", "PASHA - EPPEncode");
