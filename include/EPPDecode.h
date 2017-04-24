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

//namespace llvm {

//struct DenseMapAPIntKeyInfo {
  //static inline APInt getEmptyKey() {
    //APInt V(nullptr, 0);
    //V.VAL = 0;
    //return V;
  //}
  //static inline APInt getTombstoneKey() {
    //APInt V(nullptr, 0);
    //V.VAL = 1;
    //return V;
  //}
  //static unsigned getHashValue(const APInt &Key) {
    //return static_cast<unsigned>(hash_value(Key));
  //}
  //static bool isEqual(const APInt &LHS, const APInt &RHS) {
    //return LHS.getBitWidth() == RHS.getBitWidth() && LHS == RHS;
  //}
//};

//}


namespace epp {

enum PathType { RIRO, FIRO, RIFO, FIFO };

struct Path {
    Function *Func;
    APInt id;
    uint64_t count;
    pair<PathType, vector<BasicBlock *>> blocks;
};

struct EPPDecode : public llvm::ModulePass {
    static char ID;
    std::string filename;
    llvm::DenseMap<uint32_t, llvm::Function *> FunctionIdToPtr;
    //llvm::DenseMap<llvm::APInt, llvm::SmallVector<llvm::BasicBlock*, 16>,
        //llvm::DenseMapAPIntKeyInfo> Paths;
    

    EPPDecode(std::string f) : llvm::ModulePass(ID), filename(f) {}

    virtual void getAnalysisUsage(llvm::AnalysisUsage &au) const override {
        au.addRequired<EPPEncode>();
    }

    virtual bool runOnModule(llvm::Module &m) override;
    bool doInitialization(llvm::Module &m) override;

    std::pair<PathType, std::vector<llvm::BasicBlock *>>
    decode(llvm::Function &f, llvm::APInt pathID, EPPEncode &E);
    const char *getPassName() const override { return "EPPDecode"; }
};


}

#endif
