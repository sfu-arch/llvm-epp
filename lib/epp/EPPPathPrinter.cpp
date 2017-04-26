#define DEBUG_TYPE "epp_pathprinter"
#include "llvm/ADT/SmallString.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>

#include "EPPDecode.h"
#include "EPPPathPrinter.h"

using namespace llvm;
using namespace epp;
using namespace std;

bool EPPPathPrinter::doInitialization(Module &M) { return false; }

void printPathSrc(vector<BasicBlock *> &blocks, raw_ostream &out,
                  SmallString<8> prefix) {
    unsigned line = 0;
    llvm::StringRef file;
    for (auto *bb : blocks) {
        for (auto &instruction : *bb) {
            MDNode *n = instruction.getMetadata("dbg");
            if (!n) {
                continue;
            }
            DebugLoc Loc(n);
            if (Loc->getLine() != line || Loc->getFilename() != file) {
                line = Loc->getLine();
                file = Loc->getFilename();
                out << prefix << "- " << file.str() << "," << line << "\n";
            }
        }
    }
}

bool EPPPathPrinter::runOnModule(Module &M) {
    EPPDecode &D = getAnalysis<EPPDecode>();

    errs() << "# Decoded Paths\n";

    for (auto &F : M) {
        if (!F.isDeclaration()) {
            errs() << "- name: " << F.getName() << "\n";
            EPPEncode &E = getAnalysis<EPPEncode>(F);
            auto Res     = D.getPaths(F, E);
            errs() << "  num_exec_paths: " << Res.size() << "\n";
            for (auto &P : Res) {
                SmallString<16> PathId;
                P.Id.toStringSigned(PathId, 16);
                errs() << "  - path: " << PathId << "\n";
                printPathSrc(P.Blocks, errs(), StringRef("      "));
            }
        }
    }

    return false;
}

char EPPPathPrinter::ID = 0;
