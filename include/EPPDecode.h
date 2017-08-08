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
    std::vector<BasicBlock *> Blocks;
};

struct EPPDecode : public llvm::ModulePass {
    static char ID;
    //std::string filename;

    DenseMap<uint32_t, Function *> FunctionIdToPtr;
    llvm::DenseMap<llvm::Function *, llvm::SmallVector<Path, 16>> DecodeCache;

    EPPDecode() : llvm::ModulePass(ID) {}

    virtual void getAnalysisUsage(llvm::AnalysisUsage &au) const override {
        au.addRequired<EPPEncode>();
    }

    virtual bool runOnModule(llvm::Module &m) override;
    bool doInitialization(llvm::Module &m) override;

    SmallVector<Path, 16> getPaths(llvm::Function &F);
    void getPathInfo(uint32_t FunctionId, Path& Info);

    std::pair<PathType, std::vector<llvm::BasicBlock *>>
    decode(llvm::Function &F, llvm::APInt pathID, EPPEncode &E);

    const char *getPassName() const override { return "EPPDecode"; }
};
}

#endif
