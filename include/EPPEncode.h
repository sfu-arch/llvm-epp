#ifndef EPPENCODE_H
#define EPPENCODE_H

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"

#include <map>
#include <unordered_map>

#include "AltCFG.h"

namespace epp {

struct EPPEncode : public llvm::FunctionPass {

    static char ID;

    llvm::LoopInfo *LI;
    llvm::DenseMap<llvm::BasicBlock *, llvm::APInt> numPaths;
    altcfg ACFG;

    EPPEncode() : llvm::FunctionPass(ID), LI(nullptr) {}

    virtual void getAnalysisUsage(llvm::AnalysisUsage &au) const override {
        au.addRequired<llvm::LoopInfoWrapperPass>();
        au.setPreservesAll();
    }

    virtual bool runOnFunction(llvm::Function &f) override;
    void encode(llvm::Function &f);
    bool doInitialization(llvm::Module &m) override;
    bool doFinalization(llvm::Module &m) override;
    void releaseMemory() override;
    const char *getPassName() const override { return "PASHA - EPPEncode"; }
};
}
#endif
