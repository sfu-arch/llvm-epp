#ifndef AUXGRAPH_H
#define AUXGRAPH_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/MapVector.h"

#include <unordered_map>


using namespace llvm;

namespace aux {

struct Edge {
    BasicBlock *src, *tgt;
    bool real;
    Edge(BasicBlock* from, BasicBlock* to, bool r = true) 
        : src(from), tgt(to), real(r) {}
};

typedef std::shared_ptr<Edge> EdgePtr;

// An auxiliary graph representation of the CFG of a function which
// will be queried online during instrumentation. Edges in the graph
// will need to be updated as instrumentation changes the basic block
// pointers used to represent a particular edge/node. 
class AuxGraph {

    Function &F;
    SmallVector<BasicBlock*, 32> Nodes;
    DenseMap<const BasicBlock*, SmallVector<EdgePtr, 4>> EdgeList; 
    SmallVector<EdgePtr, 4> SegmentList; 
    std::unordered_map<EdgePtr, std::pair<EdgePtr, EdgePtr>> SegmentMap;
    std::unordered_map<EdgePtr, APInt> Weights;

    public:
        AuxGraph(Function &f);
        void dot(raw_ostream &os);
        void add(BasicBlock *src, BasicBlock *tgt);
        void segment(DenseSet<std::pair<const BasicBlock *, const BasicBlock *>> &List);
        SmallVector<EdgePtr, 4> succs(BasicBlock* B);

        SmallVector<BasicBlock*, 32> nodes() { return Nodes; }
        APInt& operator[](const EdgePtr &E) { return Weights[E]; }
};


}
#endif
