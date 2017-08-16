#ifndef SPLITLANDINGPADPREDSPASS_H
#define SPLITLANDINGPADPREDSPASS_H

#include "llvm/Pass.h"

using namespace llvm;

namespace epp {

struct SplitLandingPadPredsPass : public ModulePass {
    static char ID;
    SplitLandingPadPredsPass() : llvm::ModulePass(ID){}
    virtual bool runOnModule(llvm::Module &m) override;
    llvm::StringRef getPassName() const override { return "SplitLandingPadPredsPass"; }
};

}

#endif
