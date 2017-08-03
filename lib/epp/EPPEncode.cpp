#define DEBUG_TYPE "epp_encode"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/SetVector.h"
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

#include "EPPEncode.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <set>
#include <stack>
#include <unordered_set>
#include <vector>

// Testing
#include "AuxGraph.h"
using namespace aux;

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
    //ACFG.clear();
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

    //auto POB   = postOrder(F);
    //auto Entry = POB.back(), Exit = POB.front();
    auto BackEdges = getBackEdges(&F.getEntryBlock());

    DenseSet<std::pair<const BasicBlock *, const BasicBlock *>> SegmentEdges;

    // Add real edges
    for (auto &BB : AG.nodes()) {
        for (auto S = succ_begin(BB), E = succ_end(BB); S != E; S++) {
            if (BackEdges.count(make_pair(BB, *S)) ||
                LI->getLoopFor(BB) != LI->getLoopFor(*S)) {
                //DEBUG(errs() << "Adding segmented edge : " << BB->getName()
                             //<< " " << S->getName() << " " << Entry->getName()
                             //<< " " << Exit->getName() << "\n");
                SegmentEdges.insert({BB, *S});
                //ACFG.add(BB, *S, Entry, Exit);
                //continue;
            }
            //DEBUG(errs() << "Adding Real edge : " << BB->getName() << " "
                         //<< S->getName() << "\n");
            //ACFG.add(BB, *S);
        }
    }

    AG.segment(SegmentEdges);

    // If the function has only one basic block, then there are no edges added.
    // Handle this case separately by telling the AltCFG structure.

    //if (Entry == Exit) {
        //ACFG.add(Entry);
    //}


    //for (auto &B : POB) {
        //APInt pathCount(128, 0, true);

        //if (isFunctionExiting(B))
            //pathCount = 1;

        //for (auto &S : ACFG.succs(B)) {
            //ACFG[{B, S}] = pathCount;
            //if (numPaths.count(S) == 0)
                //numPaths.insert(make_pair(S, APInt(128, 0, true)));

            //// This is the only place we need to check for overflow.
            //bool Ov   = false;
            //pathCount = pathCount.sadd_ov(numPaths[S], Ov);
            //if (Ov) {
                //report_fatal_error("Integer Overflow");
            //}
        //}
        //numPaths.insert({B, pathCount});
    //}

    //if (numPaths[Entry].getLimitedValue() == ~0ULL) { // && !wideCounter) {
        //report_fatal_error("Numpaths greater than 2^64, please use -w option "
                           //"for 128 bit counters");
    //}

    //llvm::DenseMap<llvm::BasicBlock *, llvm::APInt> numPathsA;
    for (auto &B : AG.nodes()) {
        APInt pathCount(128, 0, true);

        if (isFunctionExiting(B))
            pathCount = 1;

        for (auto &SE : AG.succs(B)) {
            AG[SE] = pathCount;
            auto *S = SE->tgt;
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

//    error_code EC;
//    raw_fd_ostream out("auxgraph.dot", EC, sys::fs::F_Text);
//    AG.dot(out);
//    out.close();


    //raw_fd_ostream out2("acfg.dot", EC, sys::fs::F_Text);
    //ACFG.dot(out2);
    //out2.close();

    //errs() << numPathsA[Entry] << " " <<  numPaths[Entry]  << "\n";
}

char EPPEncode::ID = 0;
static RegisterPass<EPPEncode> X("", "EPPEncode");
