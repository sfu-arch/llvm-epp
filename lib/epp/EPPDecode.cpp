#define DEBUG_TYPE "epp_decode"
#include "llvm/ADT/SmallString.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "EPPDecode.h"

#include <fstream>
#include <sstream>

using namespace llvm;
using namespace epp;
using namespace std;

namespace {

inline bool isExitBlock(BasicBlock *BB) {
    if (BB->getTerminator()->getNumSuccessors() == 0)
        return true;
    return false;
}

}

bool EPPDecode::doInitialization(Module &M) {
    uint32_t Id = 0;
    for (auto &F : M) {
        FunctionIdToPtr[Id++] = &F;
    }
    return false;
}

bool EPPDecode::runOnModule(Module &M) { return false; }

void EPPDecode::getPathInfo(uint32_t FunctionId, Path &Info) {
    auto &F        = *FunctionIdToPtr[FunctionId];
    EPPEncode &Enc = getAnalysis<EPPEncode>(F);
    auto R         = decode(F, Info.Id, Enc);
    Info.Type      = R.first;
    Info.Blocks    = R.second;
}

pair<PathType, vector<BasicBlock *>>
EPPDecode::decode(Function &F, APInt pathID, EPPEncode &E) {
    vector<BasicBlock *> Sequence;
    auto *Position = &F.getEntryBlock();

    auto &AG = E.AG;

    DEBUG(errs() << "Decode Called On: " << pathID << "\n");

    vector<EdgePtr> SelectedEdges;
    while (true) {
        Sequence.push_back(Position);
        if (isExitBlock(Position))
            break;
        APInt Wt(64, 0, true);
        EdgePtr Select = nullptr;
        DEBUG(errs() << Position->getName() << " (\n");
        for (auto &Edge : AG.succs(Position)) {
            auto EWt = AG.getEdgeWeight(Edge);
            if (EWt.uge(Wt) && EWt.ule(pathID)) {
                DEBUG(errs() << "\t" << Edge->tgt->getName() << " [" << EWt
                             << "]\n");
                Select = Edge;
                Wt     = EWt;
            }
        }
        DEBUG(errs() << " )\n\n\n");

        SelectedEdges.push_back(Select);
        Position = Select->tgt;
        pathID -= Wt;
    }

    if (SelectedEdges.empty())
        return {RIRO, Sequence};

#define SET_BIT(n, x) (n |= 1ULL << x)
    uint64_t Type = 0;
    if (!SelectedEdges.front()->real) {
        SET_BIT(Type, 0);
    }
    if (!SelectedEdges.back()->real) {
        SET_BIT(Type, 1);
    }
#undef SET_BIT

    return {static_cast<PathType>(Type),
            vector<BasicBlock *>(Sequence.begin() + bool(Type & 0x1),
                                 Sequence.end() - bool(Type & 0x2))};
}

char EPPDecode::ID = 0;
