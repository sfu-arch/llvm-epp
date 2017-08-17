#ifndef AUXGRAPH_H
#define AUXGRAPH_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include <unordered_map>

using namespace llvm;

namespace epp {

struct Edge {
    BasicBlock *src, *tgt;
    bool real;
    Edge(BasicBlock *from, BasicBlock *to, bool r = true)
        : src(from), tgt(to), real(r) {}
};

typedef std::shared_ptr<Edge> EdgePtr;

// An auxiliary graph representation of the CFG of a function which
// will be queried online during instrumentation. Edges in the graph
// will need to be updated as instrumentation changes the basic block
// pointers used to represent a particular edge/node.
class AuxGraph {

    SmallVector<BasicBlock *, 32> Nodes;
    DenseMap<const BasicBlock *, SmallVector<EdgePtr, 4>> EdgeList;
    std::unordered_map<EdgePtr, std::pair<EdgePtr, EdgePtr>> SegmentMap;
    std::unordered_map<EdgePtr, APInt> Weights;
    BasicBlock *FakeExit;

  public:
    void clear();
    void init(Function &F);
    EdgePtr add(BasicBlock *src, BasicBlock *tgt, bool isReal = true);
    void
    segment(SetVector<std::pair<const BasicBlock *, const BasicBlock *>> &List);
    //void printWeights();
    void dot(raw_ostream &os) const;
    void dotW(raw_ostream &os) const;
    SmallVector<EdgePtr, 4> succs(BasicBlock *B) const;
    SmallVector<std::pair<EdgePtr, APInt>, 16> getWeights() const;
    APInt getEdgeWeight(const EdgePtr &Ptr) const;
    std::unordered_map<EdgePtr, std::pair<EdgePtr, EdgePtr>> getSegmentMap() const;
    EdgePtr exists(BasicBlock *Src, BasicBlock *Tgt, bool isReal) const;
    EdgePtr getOrInsertEdge(BasicBlock *Src, BasicBlock *Tgt, bool isReal);

    bool isExitBlock(BasicBlock *B) const { return B == FakeExit; }
    SmallVector<BasicBlock *, 32> nodes() const { return Nodes; }
    APInt &operator[](const EdgePtr &E) { return Weights[E]; }
};
}
#endif
