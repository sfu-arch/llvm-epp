#define DEBUG_TYPE "epp_decode"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>

#include <unordered_map>

#include "Common.h"
#include "EPPDecode.h"

using namespace llvm;
using namespace epp;
using namespace std;

extern cl::list<string> FunctionList;
extern bool isTargetFunction(const Function &, const cl::list<string> &);
extern cl::opt<string> profile;
extern cl::opt<bool> printSrcLines;

void printPath(vector<llvm::BasicBlock *> &Blocks, ofstream &Outfile) {
    for (auto *BB : Blocks) {
        DEBUG(errs() << BB->getName() << " ");
        Outfile << BB->getName().str() << " ";
    }
}

struct Path {
    Function *Func;
    APInt id;
    uint64_t count;
    pair<PathType, vector<BasicBlock *>> blocks;
};

static bool isFunctionExiting(BasicBlock *BB) {
    if (BB->getTerminator()->getNumSuccessors() == 0)
        return true;
    return false;
}

static uint64_t pathCheck(vector<BasicBlock *> &Blocks) {
    // Check for un-acceleratable paths,
    // a) Indirect Function Calls
    // b) Function calls to external libraries
    // c) Memory allocations
    // return 0 if un-acceleratable or num_ins otherwise

    uint64_t NumIns = 0;
    for (auto BB : Blocks) {
        for (auto &I : *BB) {
            CallSite CS(&I);
            if (CS.isCall() || CS.isInvoke()) {
                if (!CS.getCalledFunction()) {
                    errs() << "Found indirect call\n";
                    return 0;
                } else {
                    if (CS.getCalledFunction()->isDeclaration() &&
                        common::checkIntrinsic(CS)) {
                        DEBUG(errs() << "Lib Call: "
                                     << CS.getCalledFunction()->getName()
                                     << "\n");
                        return 0;
                    }
                }
            }
        }
        uint64_t N = BB->getInstList().size();
        NumIns += N;
    }

    return NumIns;
}

bool EPPDecode::runOnModule(Module &M) {
    ifstream inFile(profile.c_str(), ios::in);
    assert(inFile.is_open() && "Could not open file for reading");

    uint64_t totalPathCount;
    inFile >> totalPathCount;

    vector<Path> paths;
    paths.reserve(totalPathCount);

    EPPEncode *Enc = nullptr;
    for (auto &F : M) {
        if (isTargetFunction(F, FunctionList)) {
            Enc = &getAnalysis<EPPEncode>(F);
            vector<uint64_t> counts(totalPathCount, 0);
            string PathIdStr;
            uint64_t PathCount;
            while (inFile >> PathIdStr >> PathCount) {
                APInt PathId(128, StringRef(PathIdStr), 16);
                paths.push_back({&F, PathId, PathCount});
            }
        }
    }
    inFile.close();

    // vector<pair<PathType, vector<llvm::BasicBlock *>>>
    // bbSequences;
    // bbSequences.reserve(totalPathCount);
    // for (auto &path : paths) {
    // bbSequences.push_back(decode(*path.Func, path.id, *Enc));
    //}

    for (auto &path : paths) {
        path.blocks = decode(*path.Func, path.id, *Enc);
    }

    // Sort the paths in descending order of their frequency
    // If the frequency is same, descending order of id (id cannot be same)
    sort(paths.begin(), paths.end(), [](const Path &P1, const Path &P2) {
        return (P1.count > P2.count) ||
               (P1.count == P2.count && P1.id.uge(P2.id));
    });

    ofstream Outfile("epp-sequences.txt", ios::out);

    uint64_t pathFail = 0;
    // Dump paths
    // for (size_t i = 0, e = bbSequences.size(); i < e; ++i) {
    for (auto &path : paths) {
        auto pType = path.blocks.first;
        int start = 0, end = 0;
        switch (pType) {
        case RIRO:
            break;
        case FIRO:
            start = 1;
            break;
        case RIFO:
            end = 1;
            break;
        case FIFO:
            start = 1;
            end   = 1;
            break;
        }
        vector<BasicBlock *> blocks(path.blocks.second.begin() + start,
                                    path.blocks.second.end() - end);

        if (auto Count = pathCheck(blocks)) {
            DEBUG(errs() << path.count << " ");
            Outfile << path.id.toString(10, false) << " " << path.count << " ";
            Outfile << static_cast<int>(pType) << " ";
            Outfile << Count << " ";
            printPath(blocks, Outfile);
            Outfile << "\n";
        } else {
            pathFail++;
            DEBUG(errs() << "Path Fail\n");
        }
        DEBUG(errs() << "Path ID: " << path.id.toString(10, false)
                     << " Freq: " << path.count << "\n");

        if (printSrcLines) {
            // TODO : Cleanup -- change the declaration on line 155 to SetVector
            SetVector<BasicBlock *> SetBlocks(blocks.begin(), blocks.end());
            common::printPathSrc(SetBlocks);
        }
        DEBUG(errs() << "\n");
    }

    DEBUG(errs() << "Path Check Fails : " << pathFail << "\n");

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
static RegisterPass<EPPDecode> X("", "PASHA - EPPDecode");
