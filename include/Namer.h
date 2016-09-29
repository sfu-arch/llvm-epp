#ifndef NAMER_H
#define NAMER_H

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

namespace epp {

struct Namer : public ModulePass {
    static char ID;

    Namer() : ModulePass(ID) {}

    virtual bool doInitialization(Module &M) override;

    virtual bool doFinalization(Module &M) override { return false; }

    virtual bool runOnModule(Module &M) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override {}
};
}

#endif
