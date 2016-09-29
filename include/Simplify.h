#ifndef CLEAN_H
#define CLEAN_H

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace pasha {

struct Simplify : public ModulePass {
    static char ID;
    StringRef FunctionName;

    Simplify(StringRef FN) : ModulePass(ID), FunctionName(FN) {}

    virtual bool doInitialization(Module &M) override;
    virtual bool doFinalization(Module &M) override;
    virtual bool runOnModule(Module &M) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override {}
};
}

#endif
