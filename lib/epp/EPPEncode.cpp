#define DEBUG_TYPE "epp_encode"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/CFGPrinter.h"

#include "AuxGraph.h"
#include "EPPEncode.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <set>
#include <vector>

using namespace llvm;
using namespace epp;
using namespace std;


bool EPPEncode::doInitialization(Module &m) { return false; }
bool EPPEncode::doFinalization(Module &m) { return false; }

namespace {

void printCFG(Function &F) {
    legacy::FunctionPassManager FPM(F.getParent());
    FPM.add(llvm::createCFGPrinterPass());
    FPM.doInitialization();
    FPM.run(F);
    FPM.doFinalization();
}

// bool isFunctionExiting(BasicBlock *BB) {
//     if (BB->getTerminator()->getNumSuccessors() == 0) {
//         //errs() << BB->getName() << " is an exit block\n";
//         return true;
//     }
// 
//     return false;
// }

}

bool EPPEncode::runOnFunction(Function &F) {
    LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    encode(F);
    return false;
}

void EPPEncode::releaseMemory() {
    LI = nullptr;
    numPaths.clear();
    AG.clear();
}

void postorderHelper(BasicBlock *toVisit, vector<BasicBlock *> &blocks,
                     DenseSet<BasicBlock *> &seen,
                     const set<BasicBlock *> &SCCBlocks) {
    seen.insert(toVisit);
    for (auto s = succ_begin(toVisit), e = succ_end(toVisit); s != e; ++s) {

        // Don't need to worry about backedge successors as their targets
        // will be visited already and will fail the first condition check.

        if (!seen.count(*s) && (SCCBlocks.find(*s) != SCCBlocks.end())) {
            postorderHelper(*s, blocks, seen, SCCBlocks);
        }
    }
    blocks.push_back(toVisit);
}

vector<BasicBlock *> postOrder(BasicBlock *Block,
                               const set<BasicBlock *> &SCCBlocks) {
    vector<BasicBlock *> ordered;
    DenseSet<BasicBlock *> seen;
    postorderHelper(Block, ordered, seen, SCCBlocks);
    return ordered;
}

vector<BasicBlock *> postOrder(Function &F) {
    vector<BasicBlock *> PostOrderBlocks;

    // Add all the basicblocks in the function to a set.
    SetVector<BasicBlock *> BasicBlocks;
    for (auto &BB : F)
        BasicBlocks.insert(&BB);

    for (auto I = scc_begin(&F), IE = scc_end(&F); I != IE; ++I) {

        // Obtain the vector of BBs
        const std::vector<BasicBlock *> &SCCBBs = *I;

        // Find an entry block into the current SCC as the starting point
        // of the DFS for the postOrder traversal. Now the *entry* block
        // should have predecessors in other SCC's which are topologically
        // before this SCC, i.e blocks not seen yet.

        // Remove each basic block which we encounter in the current SCC.
        // Remaining blocks must be topologically earlier than the blocks
        // we have seen already.
        DEBUG(errs() << "SCC: ");
        for (auto *BB : SCCBBs) {
            DEBUG(errs() << BB->getName() << " ");
            BasicBlocks.remove(BB);
        }
        DEBUG(errs() << "\n");

        // Find the first block in the current SCC to have a predecessor
        // in the remaining blocks. This becomes the starting block for the DFS
        // Exception: SCC size = 1.

        BasicBlock *Start = nullptr;

        for (int i = 0; i < SCCBBs.size() && Start == nullptr; i++) {
            auto BB = SCCBBs[i];
            for (auto P = pred_begin(BB), E = pred_end(BB); P != E; P++) {
                if (BasicBlocks.count(*P)) {
                    Start = BB;
                    break;
                }
            }
        }

        if (!Start) {
            Start = SCCBBs[0];
            assert(SCCBBs.size() == 1 &&
                   "Should be entry block only which has no preds");
        }

        set<BasicBlock *> SCCBlocksSet(SCCBBs.begin(), SCCBBs.end());
        auto blocks = postOrder(Start, SCCBlocksSet);

        assert(SCCBBs.size() == blocks.size() &&
               "Could not discover all blocks");

        for (auto *BB : blocks) {
            PostOrderBlocks.push_back(BB);
        }
    }

    DEBUG(errs() << "Post Order Blocks: \n");
    for (auto &POB : PostOrderBlocks)
        DEBUG(errs() << POB->getName() << " ");
    DEBUG(errs() << "\n");

    return PostOrderBlocks;
}

DenseSet<pair<const BasicBlock *, const BasicBlock *>>
getBackEdges(BasicBlock *StartBB) {
    SmallVector<std::pair<const BasicBlock *, const BasicBlock *>, 8>
        BackEdgesVec;
    FindFunctionBackedges(*StartBB->getParent(), BackEdgesVec);
    DenseSet<pair<const BasicBlock *, const BasicBlock *>> BackEdges;

    for (auto &BE : BackEdgesVec) {
        BackEdges.insert(BE);
    }
    return BackEdges;
}

void EPPEncode::encode(Function &F) {
    DEBUG(errs() << "Called Encode on " << F.getName() << "\n");

    AG.init(F);

    auto *Entry    = &F.getEntryBlock();
    auto BackEdges = getBackEdges(Entry);

    SetVector<std::pair<const BasicBlock *, const BasicBlock *>> SegmentEdges;

    for (auto &BB : AG.nodes()) {
        for (auto S = succ_begin(BB), E = succ_end(BB); S != E; S++) {
            if (BackEdges.count(make_pair(BB, *S)) ||
                LI->getLoopFor(BB) != LI->getLoopFor(*S)) {
                SegmentEdges.insert({BB, *S});
            }
        }
    }

    // error_code EC1;
    // raw_fd_ostream out1("auxgraph-1.dot", EC1, sys::fs::F_Text);
    // AG.dot(out1);
    // out1.close();

    AG.segment(SegmentEdges);

    // error_code EC2;
    // raw_fd_ostream out2("auxgraph-2.dot", EC2, sys::fs::F_Text);
    // AG.dot(out2);
    // out2.close();

    for (auto &B : AG.nodes()) {
        APInt pathCount(64, 0, true);

        auto Succs = AG.succs(B);
        if (Succs.empty()) {
            pathCount = 1;
            assert(B->getName().startswith("fake.exit") && 
                    "The only block without a successor should be the fake exit");
        } else {
            for (auto &SE : Succs) {
                AG[SE]  = pathCount;
                auto *S = SE->tgt;
                if (numPaths.count(S) == 0)
                    numPaths.insert(make_pair(S, APInt(64, 0, true)));

                // This is the only place we need to check for overflow.
                // If there is an overflow, indicate this by saving 0 as the
                // number of paths from the entry block. This is impossible for
                // a regular CFG where the numpaths from entry would atleast be 1
                // if the entry block is also the exit block.
                bool Ov   = false;
                pathCount = pathCount.sadd_ov(numPaths[S], Ov);
                if (Ov) {
                    numPaths.clear();
                    numPaths.insert(make_pair(Entry, APInt(64, 0, true)));
                    DEBUG(errs() << "Integer Overflow in function " << F.getName());
                    return;
                }
            }
        }

        numPaths.insert({B, pathCount});
    }


    // error_code EC3;
    // raw_fd_ostream out3("auxgraph-3.dot", EC3, sys::fs::F_Text);
    // AG.dotW(out3);
    // out3.close();
}

char EPPEncode::ID = 0;
static RegisterPass<EPPEncode> X("", "EPPEncode");
