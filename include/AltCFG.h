#ifndef ALTCFG_H
#define ALTCFG_H

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/MapVector.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

#include <cassert>
#include <map>
#include <memory>
#include <set>

using namespace llvm;
using namespace std;

namespace epp {

#define SRC(E) (E.first)
#define TGT(E) (E.second)

typedef pair<BasicBlock *, BasicBlock *> Edge;
typedef SetVector<Edge> EdgeListTy;
typedef SetVector<BasicBlock *, vector<BasicBlock *>, DenseSet<BasicBlock *>>
    SuccListTy;
typedef MapVector<const BasicBlock *, SuccListTy> CFGTy;
typedef MapVector<Edge, pair<Edge, Edge>> FakeTableTy;
typedef DenseMap<const BasicBlock *, SmallVector<BasicBlock *, 4>> SuccCacheTy;
typedef MapVector<Edge, APInt> EdgeWtMapTy;

class altcfg {

    EdgeListTy Edges;
    EdgeWtMapTy Weights;
    CFGTy CFG;
    SuccCacheTy SuccCache;

    EdgeListTy get() const;
    EdgeListTy getSpanningTree(BasicBlock *);
    void spanningHelper(BasicBlock *, EdgeListTy &, DenseSet<BasicBlock *> &);
    EdgeListTy getChords(EdgeListTy &) const;
    void computeIncrement(EdgeWtMapTy &, BasicBlock *, BasicBlock *,
                          EdgeListTy &, EdgeListTy &);
    void initWt(const Edge, const APInt);

  protected:
    FakeTableTy SegmentMap;
    EdgeWtMapTy getIncrements(BasicBlock *, BasicBlock *);

  public:
    bool add(const BasicBlock*);
    bool add(BasicBlock *Src, BasicBlock *Tgt, BasicBlock *Entry = nullptr,
             BasicBlock *Exit = nullptr);
    APInt &operator[](const Edge &);
    void print(raw_ostream &os = errs()) const;
    void dot(raw_ostream &os = errs()) const;
    SmallVector<BasicBlock *, 4> succs(const BasicBlock *);
    void clear() {
        Edges.clear(), Weights.clear(), CFG.clear();
        SuccCache.clear();
    }
    EdgeListTy getFakeEdges() const;
};

typedef std::tuple<bool, APInt, bool, APInt> InstValTy;

class CFGInstHelper : public altcfg {
    EdgeWtMapTy Inc;

  public:
    CFGInstHelper(altcfg &A, BasicBlock *B, BasicBlock *C) : altcfg(A) {
        Inc = getIncrements(B, C);
    }

    void print(raw_ostream &os = errs()) {
        altcfg::print(os);
        os << "Increments: \n";
        for(auto X : Inc) {
            errs() << X.first.first << " " << X.first.second 
                << " " << X.second << "\n";
        }
    }

    InstValTy get(Edge E) const {
        auto getInc = [this](const Edge E) -> APInt {
            if (Inc.count(E))
                return Inc.lookup(E);
            return APInt(128, 0, true);
        };

        if (SegmentMap.count(E)) {
            auto F = SegmentMap.lookup(E);
            return make_tuple(true, getInc(F.first), true, getInc(F.second));
        }

        auto Increment = getInc(E);
        return Increment.ne(APInt(128, 0, true)) ? 
            make_tuple(true, Increment, false, APInt(128, 0, true)) :
            make_tuple(false, Increment, false, APInt(128, 0, true));

    }
};

}
#endif
