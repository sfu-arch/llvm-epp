#ifndef EPPDECODE_H
#define EPPDECODE_H

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "EPPEncode.h"
#include <map>
#include <vector>

namespace epp {

enum PathType { RIRO, FIRO, RIFO, FIFO };

struct Path {
    APInt Id;
    uint64_t Freq;
    PathType Type;
    vector<BasicBlock *> Blocks;
};

struct EPPDecode : public llvm::ModulePass {
    static char ID;
    std::string filename;

    llvm::DenseMap<llvm::Function *, llvm::SmallVector<Path, 16>> DecodeCache;

    EPPDecode(std::string f) : llvm::ModulePass(ID), filename(f) {}

    virtual void getAnalysisUsage(llvm::AnalysisUsage &au) const override {
        au.addRequired<EPPEncode>();
    }

    virtual bool runOnModule(llvm::Module &m) override;
    bool doInitialization(llvm::Module &m) override;

    llvm::SmallVector<Path, 16> getPaths(llvm::Function &, EPPEncode &);

    std::pair<PathType, std::vector<llvm::BasicBlock *>>
    decode(llvm::Function &f, llvm::APInt pathID, EPPEncode &E);

    const char *getPassName() const override { return "EPPDecode"; }
};
}

#endif
