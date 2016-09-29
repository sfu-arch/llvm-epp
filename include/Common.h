#ifndef COMMON_H
#define COMMON_H

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"

#include <map>
#include <sstream>
#include <string>

using namespace llvm;
using namespace std;

namespace common {

// Functions
void generateBinary(llvm::Module &m, const std::string &outputFilename,
                    char optLevel, llvm::cl::list<std::string> &libPaths,
                    llvm::cl::list<std::string> &libraries);
void saveModule(llvm::Module &m, llvm::StringRef filename);
void link(const std::string &objectFile, const std::string &outputFile,
          char optLevel, llvm::cl::list<std::string> &libPaths,
          llvm::cl::list<std::string> &libraries);
void compile(llvm::Module &, std::string, char);
llvm::DenseSet<std::pair<const llvm::BasicBlock *, const llvm::BasicBlock *>>
getBackEdges(llvm::BasicBlock *);
llvm::DenseSet<std::pair<const llvm::BasicBlock *, const llvm::BasicBlock *>>
getBackEdges(llvm::Function &);
void optimizeModule(llvm::Module *);
void lowerSwitch(llvm::Module &, llvm::StringRef);
void lowerSwitch(llvm::Function &);
void breakCritEdges(llvm::Module &, llvm::StringRef);
void breakCritEdges(llvm::Function &);
void printCFG(llvm::Function &);
bool checkIntrinsic(llvm::CallSite &);
bool isSelfLoop(const llvm::BasicBlock *);
llvm::SetVector<llvm::Loop *> getLoops(llvm::LoopInfo *);
void writeModule(llvm::Module *, std::string);
void printDFG(llvm::Function &);
void printDFG(llvm::Module &);
void labelUID(llvm::Module &);
void labelUID(llvm::Function &);
vector<BasicBlock *> postOrder(Function &, LoopInfo *);
void printPathSrc(SetVector<llvm::BasicBlock *> &, raw_ostream &out = errs());

}

namespace helpers {

// Classes
class DFGPrinter : public FunctionPass, public InstVisitor<DFGPrinter> {
    friend class InstVisitor<DFGPrinter>;
    void visitFunction(Function &F);
    void visitBasicBlock(BasicBlock &BB);
    void visitInstruction(Instruction &I);
    stringstream dot;
    map<Value *, uint64_t> nodes;
    uint64_t counter;

  public:
    static char ID;
    DFGPrinter() : FunctionPass(ID), counter(999999) {}
    bool doInitialization(Module &) override;
    bool doFinalization(Module &) override;
    bool runOnFunction(Function &) override;
    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesAll();
    }
};

class LabelUID : public FunctionPass, public InstVisitor<LabelUID> {
    friend class InstVisitor<LabelUID>;
    uint64_t counter;
    void visitFunction(Function &F);
    void visitBasicBlock(BasicBlock &BB);
    void visitInstruction(Instruction &I);
    template <typename T> void visitGeneric(string, T &);
    map<Value *, uint64_t> values;

  public:
    static char ID;
    LabelUID() : FunctionPass(ID), counter(0) {}
    bool doInitialization(Module &) override {
        counter = 0;
        values.clear();
        return false;
    };
    bool doFinalization(Module &) override { return true; };
    bool runOnFunction(Function &) override;
    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesAll();
    }
};


}

#endif
