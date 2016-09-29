#ifndef ALL_INLINER_H
#define ALL_INLINER_H
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/InlinerPass.h"
#include <fstream>


using namespace llvm;

namespace epp {

/// Inliner class which inlines everything. Very docu
///
struct PeruseInliner : public Inliner {
    static char ID;

  public:
    PeruseInliner() : Inliner(ID, -2000000000, true) {}

    ~PeruseInliner() {}

    std::ofstream InlineStats;

    InlineCost getInlineCost(CallSite CS) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override;
    bool runOnSCC(CallGraphSCC &SCC) override;

    using llvm::Pass::doInitialization;
    bool doInitialization(CallGraph &CG) override {
        return false;
    }

    using llvm::Pass::doFinalization;
    bool doFinalization(CallGraph &CG) override {
        return false;
    }
};
}
#endif
