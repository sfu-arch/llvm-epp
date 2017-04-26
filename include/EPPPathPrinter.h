#ifndef EPPPATHPRINTER_H
#define EPPPATHPRINTER_H

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "EPPDecode.h"
#include <map>
#include <vector>

namespace epp {

struct EPPPathPrinter : public llvm::ModulePass {
    static char ID;

    EPPPathPrinter() : llvm::ModulePass(ID) {}

    virtual void getAnalysisUsage(llvm::AnalysisUsage &au) const override {
        au.addRequired<EPPDecode>();
        au.addRequired<EPPEncode>();
    }

    virtual bool runOnModule(llvm::Module &m) override;
    bool doInitialization(llvm::Module &m) override;

};


}

#endif
