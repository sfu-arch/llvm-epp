#define DEBUG_TYPE "epp_auxg"
#include "AuxGraph.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <set>
#include <vector>

using namespace std;
using namespace epp;

namespace {

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

SmallVector<BasicBlock *, 32> postOrder(Function &F) {
    SmallVector<BasicBlock *, 32> PostOrderBlocks;

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
        // Exception: SCC size = 1. We rely on the fact that the ordering of
        // predecessors is deterministic.

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
}

// void AuxGraph::printWeights() {
//     for (auto &V : Weights) {
//         errs() << V.first->src->getName() << "-" << V.first->tgt->getName()
//                << ": " << V.second << "\n";
//     }
// }

/// Construct the auxiliary graph representation from the original
/// function control flow graph. At this stage the CFG and the
/// AuxGraph are the same graph.
void AuxGraph::init(Function &F) {
    Nodes = postOrder(F);
    SmallVector<BasicBlock *, 4> Leaves;
    for (auto &BB : Nodes) {
        if (BB->getTerminator()->getNumSuccessors() > 0) {
            for (auto S = succ_begin(BB), E = succ_end(BB); S != E; S++) {
                add(BB, *S);
            }
        } else {
            Leaves.push_back(BB);
        }
    }

    // Create a dummy basic block to represent the fake exit
    FakeExit = BasicBlock::Create(F.getContext(), "fake.exit");

    // For each leaf (block with no successor in the original CFG), add
    // an edge from it to the fake exit. So the only block with no successor
    // is the fake exit block wrt to the AuxGraph.
    for (auto &L : Leaves) {
        add(L, FakeExit, false);
    }

    Nodes.insert(Nodes.begin(), FakeExit);

    /// Special case when there is only 1 basic block in the function
    /// if (Nodes.size() == 1) {
    ///     auto *src = Nodes.front();
    ///     EdgeList.insert({src, SmallVector<EdgePtr, 4>()});
    /// }
}

/// Add a new edge to the edge list. This method is only used for
/// adding real edges by the constructor.
EdgePtr AuxGraph::add(BasicBlock *src, BasicBlock *tgt, bool isReal) {
    if (EdgeList.count(src) == 0) {
        EdgeList.insert({src, SmallVector<EdgePtr, 4>()});
    }
    auto E = make_shared<Edge>(src, tgt, isReal);
    EdgeList[src].push_back(E);
    return E;
}

EdgePtr AuxGraph::exists(BasicBlock *Src, BasicBlock *Tgt, bool isReal) const {
    for (auto &SE : succs(Src)) {
        if (SE->tgt == Tgt && SE->real == isReal) {
            return SE;
        }
    }
    return nullptr;
}

EdgePtr AuxGraph::getOrInsertEdge(BasicBlock *Src, BasicBlock *Tgt,
                                  bool isReal) {
    if (auto E = exists(Src, Tgt, isReal)) {
        return E;
    }
    return add(Src, Tgt, isReal);
}

/// List of edges to be *segmented*. A segmented edge is an edge which
/// exists in the original CFG but is replaced by two edges in the
/// AuxGraph. An edge from A->B, is replaced by {A->Exit, Entry->B}.
/// An edge can only be segmented once. This
void AuxGraph::segment(
    SetVector<pair<const BasicBlock *, const BasicBlock *>> &List) {
    SmallVector<EdgePtr, 4> SegmentList;
    /// Move the internal EdgePtr from the EdgeList to the SegmentList
    for (auto &L : List) {
        auto *Src = L.first, *Tgt = L.second;

        assert(EdgeList.count(Src) &&
               "Source basicblock not found in edge list.");
        auto &Edges = EdgeList[Src];
        auto it     = find_if(Edges.begin(), Edges.end(),
                          [&Tgt](EdgePtr &P) { return P->tgt == Tgt; });
        assert(it != Edges.end() &&
               "Target basicblock not found in edge list.");
        assert(SegmentMap.count(*it) == 0 &&
               "An edge can only be segmented once.");
        SegmentList.push_back(move(*it));
        Edges.erase(it);
    }

    /// Add two new edges for each edge in the SegmentList. Update the EdgeList.
    /// An edge from A->B, is replaced by {A->Exit, Entry->B}.
    auto &Entry = Nodes.back(), &Exit = Nodes.front();
    for (auto &S : SegmentList) {
        auto *A = S->src, *B = S->tgt;
        // errs() << "Segmenting: " << A->getName() << "-" << B->getName() <<
        // "\n";
        auto AExit  = make_shared<Edge>(A, Exit, false);
        auto EntryB = make_shared<Edge>(Entry, B, false);
        // errs() << "Output: " << A->getName() << "-" << Exit->getName()
        //<< "," << Entry->getName() << "-" << B->getName() << "\n";
        EdgeList[A].push_back(AExit);
        EdgeList[Entry].push_back(EntryB);
        SegmentMap.insert({S, {AExit, EntryB}});
    }
}

/// Get all non-zero weights for non-segmented edges.
SmallVector<pair<EdgePtr, APInt>, 16> AuxGraph::getWeights() const {
    SmallVector<pair<EdgePtr, APInt>, 16> Result;
    copy_if(Weights.begin(), Weights.end(), back_inserter(Result),
            [](const pair<EdgePtr, APInt> &V) {
                return V.first->real && V.second.ne(APInt(64, 0, true));
            });
    return Result;
}

/// Get the segment mapping
std::unordered_map<EdgePtr, std::pair<EdgePtr, EdgePtr>>
AuxGraph::getSegmentMap() const {
    return SegmentMap;
}

/// Get weight for a specific edge.
APInt AuxGraph::getEdgeWeight(const EdgePtr &Ptr) const {
    return Weights.at(Ptr);
}

/// Return the successors edges of a basicblock from the Auxiliary Graph.
SmallVector<EdgePtr, 4> AuxGraph::succs(BasicBlock *B) const {
    return EdgeList.lookup(B);
}

/// Print out the AuxGraph in Graphviz format. Defaults to printing to
/// llvm::errs()
void AuxGraph::dot(raw_ostream &os = errs()) const {
    os << "digraph \"AuxGraph\" {\n label=\"AuxGraph\";\n";
    for (auto &N : Nodes) {
        os << "\tNode" << N << " [shape=record, label=\"" << N->getName().str()
           << "\"];\n";
    }
    for (auto &EL : EdgeList) {
        for (auto &L : EL.getSecond()) {
            os << "\tNode" << EL.getFirst() << " -> Node" << L->tgt
               << " [style=solid,";
            if (!L->real)
                os << "color=\"red\",";
            // os << " label=\"" << Weights[L] << "\"];\n";
            os << " label=\""
               << "\"];\n";
        }
    }
    os << "}\n";
}

/// Print out the AuxGraph in Graphviz format. Defaults to printing to
/// llvm::errs()
void AuxGraph::dotW(raw_ostream &os = errs()) const {
    os << "digraph \"AuxGraph\" {\n label=\"AuxGraph\";\n";
    for (auto &N : Nodes) {
        os << "\tNode" << N << " [shape=record, label=\"" << N->getName().str()
           << "\"];\n";
    }
    for (auto &EL : EdgeList) {
        for (auto &L : EL.getSecond()) {
            os << "\tNode" << EL.getFirst() << " -> Node" << L->tgt
               << " [style=solid,";
            if (!L->real)
                os << "color=\"red\",";
            os << " label=\"" << Weights.at(L) << "\"];\n";
        }
    }
    os << "}\n";
}

/// Clear all internal state; to be called by the releaseMemory function
void AuxGraph::clear() {
    Nodes.clear(), EdgeList.clear(), SegmentMap.clear(), Weights.clear();
}
