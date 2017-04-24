#define DEBUG_TYPE "epp_decode"
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
#include <unordered_map>

#include "EPPDecode.h"

using namespace llvm;
using namespace epp;
using namespace std;

extern cl::opt<string> profile;

//struct Path {
    //Function *Func;
    //APInt id;
    //uint64_t count;
    //pair<PathType, vector<BasicBlock *>> blocks;
//};

static bool isFunctionExiting(BasicBlock *BB) {
    if (BB->getTerminator()->getNumSuccessors() == 0)
        return true;
    return false;
}

bool EPPDecode::doInitialization(Module &M) {
    uint32_t Id = 0;
    for (auto &F : M) {
        FunctionIdToPtr[Id++] = &F;
    }

    return false;
}

void printPathSrc(SetVector<llvm::BasicBlock *> &blocks, raw_ostream &out,
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

bool EPPDecode::runOnModule(Module &M) {

    ifstream InFile(filename.c_str(), ios::in);
    assert(InFile.is_open() && "Could not open file for reading");

    errs() << "# Decoded Paths\n";

    string Line;
    while (getline(InFile, Line)) {
        uint32_t FunctionId = 0, NumberOfPaths = 0;
        try {
            stringstream SS(Line);
            SS >> FunctionId >> NumberOfPaths;
        } catch (exception &E) {
            report_fatal_error("Invalid profile format");
        }

        vector<Path> paths;
        paths.reserve(NumberOfPaths);
        Function *FPtr = FunctionIdToPtr[FunctionId];

        errs() << "- name: " << FPtr->getName() << "\n";

        EPPEncode *Enc = &getAnalysis<EPPEncode>(*FPtr);

        errs() << "  num_exec_paths: " << NumberOfPaths << "\n";

        for (uint32_t I = 0; I < NumberOfPaths; I++) {
            getline(InFile, Line);
            stringstream SS(Line);
            string PathIdStr;
            uint64_t PathCount;
            SS >> PathIdStr >> PathCount;
            APInt PathId(128, StringRef(PathIdStr), 16);
            paths.push_back({FPtr, PathId, PathCount});
            paths.back().blocks = decode(*FPtr, PathId, *Enc);
        }

        // Sort the paths in descending order of their frequency
        // If the frequency is same, descending order of id (id cannot be same)
        sort(paths.begin(), paths.end(), [](const Path &P1, const Path &P2) {
            return (P1.count > P2.count) ||
                   (P1.count == P2.count && P1.id.uge(P2.id));
        });

        for (uint32_t I = 0; I < paths.size(); I++) {
            auto &path  = paths[I];
            auto pType  = path.blocks.first;
            auto blocks = SetVector<BasicBlock *>(
                path.blocks.second.begin() + bool(pType & 0x1),
                path.blocks.second.end() - bool(pType & 0x2));
            SmallString<16> PathId;
            path.id.toStringSigned(PathId, 16);
            errs() << "  - path: " << PathId << "\n";
            printPathSrc(blocks, errs(), StringRef("      "));
        }
    }

    InFile.close();
    return false;
}

pair<PathType, vector<llvm::BasicBlock *>>
EPPDecode::decode(Function &F, APInt pathID, EPPEncode &Enc) {
    vector<llvm::BasicBlock *> Sequence;
    auto *Position = &F.getEntryBlock();
    auto &ACFG     = Enc.ACFG;

    DEBUG(errs() << "Decode Called On: " << pathID << "\n");

    vector<Edge> SelectedEdges;
    while (true) {
        Sequence.push_back(Position);
        if (isFunctionExiting(Position))
            break;
        APInt Wt(128, 0, true);
        Edge Select = {nullptr, nullptr};
        DEBUG(errs() << Position->getName() << " (\n");
        for (auto *Tgt : ACFG.succs(Position)) {
            auto EWt = ACFG[{Position, Tgt}];
            DEBUG(errs() << "\t" << Tgt->getName() << " [" << EWt << "]\n");
            if (ACFG[{Position, Tgt}].uge(Wt) &&
                ACFG[{Position, Tgt}].ule(pathID)) {
                Select = {Position, Tgt};
                Wt     = ACFG[{Position, Tgt}];
            }
        }
        DEBUG(errs() << " )\n\n\n");

        SelectedEdges.push_back(Select);
        Position = TGT(Select);
        pathID -= Wt;
    }

    if (SelectedEdges.empty())
        return {RIRO, Sequence};

    auto FakeEdges = ACFG.getFakeEdges();

#define SET_BIT(n, x) (n |= 1ULL << x)
    uint64_t Type = 0;
    if (FakeEdges.count(SelectedEdges.front())) {
        SET_BIT(Type, 0);
    }
    if (FakeEdges.count(SelectedEdges.back())) {
        SET_BIT(Type, 1);
    }
#undef SET_BIT

    return make_pair(static_cast<PathType>(Type), Sequence);
}

char EPPDecode::ID = 0;
