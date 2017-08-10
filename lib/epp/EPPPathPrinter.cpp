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
#include <sstream>

#include "EPPDecode.h"
#include "EPPPathPrinter.h"

using namespace llvm;
using namespace epp;
using namespace std;

extern cl::opt<string> profile;

bool EPPPathPrinter::doInitialization(Module &M) {
    uint32_t Id = 0;
    for (auto &F : M) {
        FunctionIdToPtr[Id++] = &F;
    }
    return false;
}

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

    ifstream InFile(profile.c_str(), ios::in);
    assert(InFile.is_open() && "Could not open file for reading");

    errs() << "# Decoded Paths\n";

    try {
        string Line;
        while (getline(InFile, Line)) {
            uint32_t FunctionId = 0, NumberOfPaths = 0;
            stringstream SS(Line);
            SS >> FunctionId >> NumberOfPaths;

            // If no paths have been executed for this function,
            // then skip it altogether.

            if (NumberOfPaths == 0)
                continue;

            errs() << "- name: " << FunctionIdToPtr[FunctionId]->getName()
                   << "\n";
            errs() << "  num_exec_paths: " << NumberOfPaths << "\n";

            vector<Path> Paths;
            for (uint32_t I = 0; I < NumberOfPaths; I++) {
                getline(InFile, Line);
                stringstream SS(Line);
                string PathIdStr;
                uint64_t PathExecFreq;
                SS >> PathIdStr >> PathExecFreq;
                APInt PathId(64, StringRef(PathIdStr), 16);

                // Add a path data struct for each path we find in the
                // profile. For each struct only initialize the Id and
                // Frequency fields.
                Path P = {PathId, PathExecFreq};
                D.getPathInfo(FunctionId, P);
                Paths.push_back(P);
            }

            // Sort the paths in descending order of their frequency
            // If the frequency is same, descending order of id (id cannot be
            // same)
            sort(Paths.begin(), Paths.end(),
                 [](const Path &P1, const Path &P2) {
                     return (P1.Freq > P2.Freq) ||
                            (P1.Freq == P2.Freq && P1.Id.uge(P2.Id));
                 });

            for (auto &P : Paths) {
                SmallString<16> PathId;
                P.Id.toStringSigned(PathId, 16);
                errs() << "  - path: " << PathId << "\n";
                printPathSrc(P.Blocks, errs(), StringRef("      "));
            }
        }
    } catch (...) {
        report_fatal_error("Invalid profile format?");
    }

    InFile.close();

    return false;
}

char EPPPathPrinter::ID = 0;
