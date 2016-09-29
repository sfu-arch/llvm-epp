#define DEBUG_TYPE "pasha_statistics"

#include "Common.h"
#include "llvm/ADT/SCCIterator.h"
#include <cassert>
#include <fstream>
#include <string>

using namespace llvm;
using namespace std;
using namespace helpers;

bool BranchTaxonomy::doInitialization(Module &M) { return false; }

void BranchTaxonomy::loopBounds(Function &F) {
    LoopInfo &LI          = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
    ScalarEvolution &SCEV = getAnalysis<ScalarEvolutionWrapperPass>(F).getSE();

    uint64_t Bounded = 0, NumLoops = 0;
    for (auto &L : LI) {
        SmallVector<BasicBlock *, 4> ExitBlocks;
        L->getExitingBlocks(ExitBlocks);
        for (auto EB : ExitBlocks) {
            uint32_t Bounds = SCEV.getSmallConstantTripCount(L, EB);
            if (Bounds)
                Bounded++;
            NumLoops++;
        }
    }

    Data["num-loops"]     = NumLoops;
    Data["bounded-loops"] = Bounded;
}

void BranchTaxonomy::functionDAG(Function &F) {
    ofstream Out("fndag." + F.getName().str() + ".dot", ios::out);
    auto BackEdges = common::getBackEdges(F);

    Out << "digraph \"Function DAG\" {\n";
    for (auto &BB : F) {
        Out << "Node" << &BB << " [shape=record, label=\"" << BB.getName().str()
            << "\"";

        auto T = BB.getTerminator();
        for (unsigned I = 0; I < T->getNumSuccessors(); I++) {
            if (BackEdges.count({&BB, T->getSuccessor(I)}) == 0)
                Out << ", S" << I << "=\"Node" << T->getSuccessor(I) << "\"";
        }
        Out << "];\n";

        for (unsigned I = 0; I < T->getNumSuccessors(); I++) {
            if (BackEdges.count({&BB, T->getSuccessor(I)}) == 0)
                Out << "Node" << &BB << "->Node" << T->getSuccessor(I) << ";\n";
        }
    }
    Out << "}\n";
    Out.close();
}

void listCBR(Function &F) {
    ofstream Out("cbr.list.txt", ios::out);
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (auto *BI = dyn_cast<BranchInst>(&I)) {
                if (BI->isConditional()) {
                    Out << BB.getName().str() << "\n";
                }
            }
        }
    }
    Out.close();
}

bool BranchTaxonomy::runOnModule(Module &M) {
    for (auto &F : M) {

        if (F.getName().str() != FName)
            continue;

        loopBounds(F);
        functionDAG(F);
        listCBR(F);
    }
    return false;
}

bool BranchTaxonomy::doFinalization(Module &M) {
    ofstream Outfile("branch.stats.txt", ios::out);
    for (auto KV : Data) {
        Outfile << KV.first << " " << KV.second << "\n";
    }
    Outfile.close();
    return false;
}

char BranchTaxonomy::ID = 0;
// static RegisterPass<BranchTaxonomy> X("", "PASHA - BranchTaxonomy");
