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

#include "EPPDecode.h"

using namespace llvm;
using namespace epp;
using namespace std;
using namespace aux;

namespace {

inline bool isExitBlock(BasicBlock *BB) {
    if (BB->getTerminator()->getNumSuccessors() == 0)
        return true;
    return false;
}
}

bool EPPDecode::doInitialization(Module &M) { return false; }

bool EPPDecode::runOnModule(Module &M) {

    DenseMap<uint32_t, Function *> FunctionIdToPtr;
    uint32_t Id = 0;
    for (auto &F : M) {
        FunctionIdToPtr[Id++] = &F;
    }

    ifstream InFile(filename.c_str(), ios::in);
    assert(InFile.is_open() && "Could not open file for reading");

    string Line;
    while (getline(InFile, Line)) {
        uint32_t FunctionId = 0, NumberOfPaths = 0;
        try {
            stringstream SS(Line);
            SS >> FunctionId >> NumberOfPaths;
        } catch (exception &E) {
            report_fatal_error("Invalid profile format");
        }

        Function *FPtr = FunctionIdToPtr[FunctionId];
        assert(FPtr && "Invalid function id in path profile");

        if (DecodeCache.count(FPtr) == 0) {
            DecodeCache.insert({FPtr, SmallVector<Path, 16>()});
        }

        for (uint32_t I = 0; I < NumberOfPaths; I++) {
            getline(InFile, Line);
            stringstream SS(Line);
            string PathIdStr;
            uint64_t PathExecFreq;
            SS >> PathIdStr >> PathExecFreq;
            APInt PathId(128, StringRef(PathIdStr), 16);

            // Add a path data struct for each path we find in the
            // profile. For each struct only initialize the Id and
            // Frequency fields.
            DecodeCache[FPtr].push_back({PathId, PathExecFreq});
        }
    }

    InFile.close();

    return false;
}

llvm::SmallVector<Path, 16> EPPDecode::getPaths(llvm::Function &F,
                                                EPPEncode &Enc) {

    assert(DecodeCache.count(&F) != 0 && "Function not found!");

    // Return the predecoded paths if they are present in the cache.
    // The check is based on the fact that there exists at least one block
    // in the path thus the size of the SmallVector is non-zero. Also
    // if one path is decoded, then all paths are decoded for a function.

    if (DecodeCache[&F].front().Blocks.size() == 0) {
        for (auto &P : DecodeCache[&F]) {
            auto R   = decode(F, P.Id, Enc);
            P.Blocks = R.second;
            P.Type   = R.first;
        }
    }

    auto &R = DecodeCache[&F];

    // Sort the paths in descending order of their frequency
    // If the frequency is same, descending order of id (id cannot be same)
    sort(R.begin(), R.end(), [](const Path &P1, const Path &P2) {
        return (P1.Freq > P2.Freq) || (P1.Freq == P2.Freq && P1.Id.uge(P2.Id));
    });

    return DecodeCache[&F];
}

pair<PathType, vector<BasicBlock *>>
EPPDecode::decode(Function &F, APInt pathID, EPPEncode &Enc) {
    vector<BasicBlock *> Sequence;
    auto *Position = &F.getEntryBlock();

    auto &AG = Enc.AG;

    DEBUG(errs() << "Decode Called On: " << pathID << "\n");

    vector<EdgePtr> SelectedEdges;
    while (true) {
        Sequence.push_back(Position);
        if (isExitBlock(Position))
            break;
        APInt Wt(128, 0, true);
        EdgePtr Select = nullptr;
        DEBUG(errs() << Position->getName() << " (\n");
        for (auto &Edge : AG.succs(Position)) {
            auto EWt = AG.getEdgeWeight(Edge);
            if (EWt.uge(Wt) && EWt.ule(pathID)) {
                DEBUG(errs() << "\t" << Edge->tgt->getName() << " [" << EWt
                             << "]\n");
                Select = Edge;
                Wt     = EWt;
            }
        }
        DEBUG(errs() << " )\n\n\n");

        SelectedEdges.push_back(Select);
        Position = Select->tgt;
        pathID -= Wt;
    }

    if (SelectedEdges.empty())
        return {RIRO, Sequence};

#define SET_BIT(n, x) (n |= 1ULL << x)
    uint64_t Type = 0;
    if (!SelectedEdges.front()->real) {
        SET_BIT(Type, 0);
    }
    if (!SelectedEdges.back()->real) {
        SET_BIT(Type, 1);
    }
#undef SET_BIT

    return {static_cast<PathType>(Type),
            vector<BasicBlock *>(Sequence.begin() + bool(Type & 0x1),
                                 Sequence.end() - bool(Type & 0x2))};
}

char EPPDecode::ID = 0;
