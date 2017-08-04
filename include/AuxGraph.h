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

namespace aux {

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

  public:
    void clear();
    void init(Function &F);
    void dot(raw_ostream &os);
    void add(BasicBlock *src, BasicBlock *tgt);
    void
    segment(SetVector<std::pair<const BasicBlock *, const BasicBlock *>> &List);
    void printWeights();
    SmallVector<EdgePtr, 4> succs(BasicBlock *B);
    SmallVector<std::pair<EdgePtr, APInt>, 16> getWeights();
    APInt getEdgeWeight(const EdgePtr &Ptr);
    std::unordered_map<EdgePtr, std::pair<EdgePtr, EdgePtr>> getSegmentMap();

    SmallVector<BasicBlock *, 32> nodes() { return Nodes; }
    APInt &operator[](const EdgePtr &E) { return Weights[E]; }
};
}
#endif
