#ifndef EPPPROFILE_H
#define EPPPROFILE_H
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "EPPEncode.h"

namespace epp {
struct EPPProfile : public llvm::ModulePass {
    static char ID;

    llvm::LoopInfo *LI;

    EPPProfile() : llvm::ModulePass(ID), LI(nullptr) {}

    virtual void getAnalysisUsage(llvm::AnalysisUsage &au) const override {
        au.addRequired<llvm::LoopInfoWrapperPass>();
        au.addRequired<EPPEncode>();
    }

    virtual bool runOnModule(llvm::Module &m) override;
    void instrument(llvm::Function &F, EPPEncode &E);

    bool doInitialization(llvm::Module &m);
    bool doFinalization(llvm::Module &m);
};
}

#endif
