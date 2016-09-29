#include "Namer.h"
#include <cassert>
#include <string>

#define DEBUG_TYPE "namer"

using namespace llvm;
using namespace std;
using namespace epp;

extern cl::list<std::string> FunctionList;
extern bool isTargetFunction(const Function &, const cl::list<std::string> &);

bool Namer::doInitialization(Module &M) {
    assert(FunctionList.size() == 1 && "Can only patch one function at a time");
    return false;
}

bool Namer::runOnModule(Module &M) {
    uint64_t Counter = 0;
    for (auto &F : M) {
        if (isTargetFunction(F, FunctionList)) {
            for (auto &BB : F) {
                if (BB.getName().str() == string("")) {
                    BB.setName(string("__unk__") + to_string(Counter));
                    Counter++;
                }
            }
        }
    }
    return true;
}

char Namer::ID = 0;
static RegisterPass<Namer> X("", "PASHA - Namer");
