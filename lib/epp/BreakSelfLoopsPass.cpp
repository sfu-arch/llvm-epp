#include "BreakSelfLoopsPass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace epp;

namespace {

void BreakSelfLoops(Function &F) {
    for (auto &BB : F) {
         
    }
}

}

/// While BreakCriticalEdges should break most self loops, sometimes it
/// does not if the self loop does not have a successor apart from itself,
/// ie. it is an infinite loop. We find this to occur in 401.bzip2,
/// handle_compress. 
bool BreakSelfLoopsPass::runOnModule(Module &M) {
    for (auto &F : M) {
        if (F.isDeclaration())
            continue;
        BreakSelfLoops(F);
    }

    return true;
}

char BreakSelfLoopsPass::ID = 0;
