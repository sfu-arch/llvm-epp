#include "BreakSelfLoopsPass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace epp;

namespace {

bool BreakSelfLoops(Function &F) {
    bool Changed = false;

    SmallVector<BasicBlock*, 4> SelfLoops;
    for (auto &BB : F) {
        for(auto S = succ_begin(&BB), E = succ_end(&BB); S != E; S++) {
            if(*S == &BB) 
                SelfLoops.push_back(&BB);
        }            
    }

    for(auto BB : SelfLoops) {
        Changed |= (bool)SplitEdge(BB, BB);
    }

    return Changed;
}

}

/// While BreakCriticalEdges should break most self loops, sometimes it
/// does not if the self loop does not have a successor apart from itself,
/// ie. it is an infinite loop. We find this to occur in 401.bzip2,
/// handle_compress. 
bool BreakSelfLoopsPass::runOnModule(Module &M) {
    bool Changed = false;
    for (auto &F : M) {
        if (F.isDeclaration())
            continue;
        Changed |= BreakSelfLoops(F);
    }

    return Changed;
}

char BreakSelfLoopsPass::ID = 0;
