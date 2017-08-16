#ifndef BREAKSELFLOOPSPASS_H
#define BREAKSELFLOOPSPASS_H

#include "llvm/Pass.h"

using namespace llvm;

namespace epp {

struct BreakSelfLoopsPass : public ModulePass {
    static char ID;
    BreakSelfLoopsPass() : llvm::ModulePass(ID){}
    virtual bool runOnModule(llvm::Module &m) override;
    llvm::StringRef getPassName() const override { return "BreakSelfLoopsPass"; }
};

}

#endif
