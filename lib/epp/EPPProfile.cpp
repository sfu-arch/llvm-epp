#define DEBUG_TYPE "epp_profile"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CFG.h"
//#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "EPPEncode.h"
#include "EPPProfile.h"

#include <cassert>
#include <tuple>
#include <unordered_map>

using namespace llvm;
using namespace epp;
using namespace std;

extern cl::opt<string> profileOutputFilename;

bool EPPProfile::doInitialization(Module &M) {
    uint32_t Id = 0;
    for (auto &F : M) {
        FunctionIds[&F] = Id++;
    }

    return false;
}

bool EPPProfile::doFinalization(Module &M) { return false; }

namespace {

uint64_t NumInstInc = 0;
uint64_t NumInstLog = 0;

void saveModule(Module &m, StringRef filename) {
    error_code EC;
    raw_fd_ostream out(filename.data(), EC, sys::fs::F_None);

    if (EC) {
        report_fatal_error("error saving llvm module to '" + filename +
                           "': \n" + EC.message());
    }
    WriteBitcodeToFile(&m, out);
}

SmallVector<BasicBlock *, 1> getFunctionExitBlocks(Function &F) {
    SmallVector<BasicBlock *, 1> R;
    for (auto &BB : F) {
        if (BB.getTerminator()->getNumSuccessors() == 0) {
            R.push_back(&BB);
        }
    }
    return R;
}

void insertInc(BasicBlock *Block, APInt Inc, AllocaInst *Ctr) {
    if (Inc.ne(APInt(64, 0, true))) {
        //(errs() << "Inserting Increment " << Increment << " "
        //<< addPos->getParent()->getName() << "\n");
        auto *addPos = &*Block->getFirstInsertionPt();
        auto *LI     = new LoadInst(Ctr, "ld.epp.ctr", addPos);

        Constant *CI =
            ConstantInt::getIntegerValue(Ctr->getAllocatedType(), Inc);
        auto *BI = BinaryOperator::CreateAdd(LI, CI);
        BI->insertAfter(LI);
        (new StoreInst(BI, Ctr))->insertAfter(BI);

        ++NumInstInc;
    }
}

BasicBlock *interpose(BasicBlock *BB, BasicBlock *Succ,
                      DominatorTree *DT = nullptr, LoopInfo *LI = nullptr) {

    unsigned SuccNum = GetSuccessorNumber(BB, Succ);

    // If this is a critical edge, let SplitCriticalEdge do it. (This does
    // not deal with critical edges which terminate at ehpads)
    TerminatorInst *LatchTerm = BB->getTerminator();
    if (SplitCriticalEdge(
            LatchTerm, SuccNum,
            CriticalEdgeSplittingOptions(DT, LI).setPreserveLCSSA())) {
        return LatchTerm->getSuccessor(SuccNum);
    }

    // If the edge isn't critical, then BB has a single successor or Succ has a
    // single pred. Insert a new block or split the pred block.
    if (BasicBlock *SP = Succ->getSinglePredecessor()) {
        assert(SP == BB && "CFG broken");
        auto *New = BasicBlock::Create(
            BB->getContext(), BB->getName() + ".intp", BB->getParent());

        BB->getTerminator()->replaceUsesOfWith(Succ, New);
        BranchInst::Create(Succ, New);

        // Hoist all special instructions from the Tgt block
        // to the new block. Rewrite the uses of the old instructions
        // to use the instructions in the new block. This is used when the
        // edge being split has ehpad destination.

        for (auto &I : vector<BasicBlock::iterator>(
                 Succ->begin(), Succ->getFirstInsertionPt())) {
            I->moveBefore(New->getTerminator());
        }

        return New;
    }

    // Otherwise, if BB has a single successor, successorsplit it at the bottom
    // of the block.
    assert(BB->getTerminator()->getNumSuccessors() == 1 &&
           "Should have a single succ!");
    return SplitBlock(BB, BB->getTerminator(), DT, LI);
}

void insertLogPath(BasicBlock *BB, uint64_t FuncId, AllocaInst *Ctr,
                   Constant *Zap) {

    //errs() << "Inserting Log: " << BB->getName() << "\n";
    //errs() << *BB << "\n";

    Module *M    = BB->getModule();
    auto &Ctx    = M->getContext();
    auto *voidTy = Type::getVoidTy(Ctx);
    auto *CtrTy  = Ctr->getAllocatedType();
    auto *FIdArg = ConstantInt::getIntegerValue(CtrTy, APInt(64, FuncId, true));
    Function *logFun2 = cast<Function>(
        M->getOrInsertFunction("__epp_logPath", voidTy, CtrTy, CtrTy));


    // We insert the logging function as the first thing in the basic block
    // as we know for sure that there is no other instrumentation present in
    // this basic block.
    Instruction *logPos = &*BB->getFirstInsertionPt();
    auto *LI               = new LoadInst(Ctr, "ld.epp.ctr", logPos);
    vector<Value *> Params = {LI, FIdArg};
    auto *CI               = CallInst::Create(logFun2, Params, "");
    CI->insertAfter(LI);
    (new StoreInst(Zap, Ctr))->insertAfter(CI);


    ++NumInstLog;
}

}

void EPPProfile::addCtorsAndDtors(Module &Mod) {
    auto &Ctx                  = Mod.getContext();
    auto *voidTy               = Type::getVoidTy(Ctx);
    auto *int32Ty              = Type::getInt32Ty(Ctx);
    auto *int8PtrTy            = Type::getInt8PtrTy(Ctx, 0);
    uint32_t NumberOfFunctions = FunctionIds.size();

    auto *EPPInit = cast<Function>(
        Mod.getOrInsertFunction("__epp_init", voidTy, int32Ty));
    auto *EPPSave = cast<Function>(
        Mod.getOrInsertFunction("__epp_save", voidTy, int8PtrTy));

    // Add Global Constructor for initializing path profiling
    auto *EPPInitCtor =
        cast<Function>(Mod.getOrInsertFunction("__epp_ctor", voidTy));
    auto *CtorBB = BasicBlock::Create(Ctx, "entry", EPPInitCtor);
    auto *Arg    = ConstantInt::get(int32Ty, NumberOfFunctions, false);
    CallInst::Create(EPPInit, {Arg}, "", CtorBB);
    ReturnInst::Create(Ctx, CtorBB);
    appendToGlobalCtors(Mod, EPPInitCtor, 0);

    auto *Number = ConstantInt::get(int32Ty, NumberOfFunctions, false);
    new GlobalVariable(Mod, Number->getType(), false,
                       GlobalValue::ExternalLinkage, Number,
                       "__epp_numberOfFunctions");

    // Add global destructor to dump out results
    auto *EPPSaveDtor =
        cast<Function>(Mod.getOrInsertFunction("__epp_dtor", voidTy));
    auto *DtorBB = BasicBlock::Create(Ctx, "entry", EPPSaveDtor);
    IRBuilder<> Builder(DtorBB);
    Builder.CreateCall(
        EPPSave,
        {Builder.CreateGlobalStringPtr(profileOutputFilename.getValue())});
    Builder.CreateRet(nullptr);

    appendToGlobalDtors(Mod, cast<Function>(EPPSaveDtor), 0);
}

bool EPPProfile::runOnModule(Module &Mod) {
    DEBUG(errs() << "Running Profile\n");

    errs() << "# Instrumented Functions\n";

    for (auto &F : Mod) {
        if (F.isDeclaration())
            continue;

        auto &Enc     = getAnalysis<EPPEncode>(F);
        auto NumPaths = Enc.numPaths[&F.getEntryBlock()];

        errs() << "- name: " << F.getName() << "\n";
        errs() << "  num_paths: " << NumPaths << "\n";
        // Check if integer overflow occurred during path enumeration,
        // if it did then the entry block numpaths is set to zero.
        if (NumPaths.ne(APInt(64, 0, true))) {
            instrument(F, Enc);
            errs() << "  num_inst_inc: " << NumInstInc << "\n";
            errs() << "  num_inst_log: " << NumInstLog << "\n";
        }
    }

    addCtorsAndDtors(Mod);

    return true;
}

/// This routine inserts two types of instrumentation.
/// 1. Incrementing a counter along a set of edges
/// 2. Logging the value of the counter at certain blocks.
/// For 1) The counter is incremented by splitting an existing
/// edge in the CFG. This implies a new basic block is inserted
/// between two basic blocks and the instrumentation is inserted
/// into the new block.
/// For 2) The counter value is saved by the runtim at certain
/// basic blocks. This is performed by the insertion of function
/// call to the logging runtime function.
/// Goals:
/// 1) Splitting edges should insert *new* blocks inside them so
/// that the Graph structure which maintains the edge weights does
/// not need to be updated at runtime.
/// 2) Instrumentation is always inserted at the BB's first insertion pt.
/// 3) There is an ordering on the the instrumentation insertion.
///   - splitting edges
///   - leaf log function calls
///   - counter allocation
void EPPProfile::instrument(Function &F, EPPEncode &Enc) {
    NumInstInc = 0, NumInstLog = 0;

    Module *M       = F.getParent();
    auto &Ctx       = M->getContext();
    uint64_t FuncId = FunctionIds[&F];
    const DataLayout& DL = M->getDataLayout();

    // Allocate a counter but dont insert it just yet. We want the
    // counter to be the last thing to insert in the function so that
    // it always dominates the log function call -- eg. when there is
    // only 1 basic block in the function.
    Type *CtrTy   = Type::getInt64Ty(Ctx);
    Constant *Zap = ConstantInt::getIntegerValue(CtrTy, APInt(64, 0, true));
    auto *Ctr     = new AllocaInst(CtrTy, DL.getAllocaAddrSpace(), nullptr, "epp.ctr");

    auto ExitBlocks = getFunctionExitBlocks(F);

    // Get all the non-zero real edges to instrument
    const auto &Wts = Enc.AG.getWeights();

    // Enc.AG.printWeights();

    for (auto &W : Wts) {
        auto &Ptr       = W.first;
        BasicBlock *Src = Ptr->src, *Tgt = Ptr->tgt;
        BasicBlock *N = interpose(Src, Tgt);
        insertInc(N, W.second, Ctr);
    }

    // Get the weights for the segmented edges
    const auto &SegMap = Enc.AG.getSegmentMap();

    for (auto &S : SegMap) {
        auto &Ptr       = S.first;
        BasicBlock *Src = Ptr->src, *Tgt = Ptr->tgt;

        auto &AExit  = S.second.first;
        APInt Pre    = Enc.AG.getEdgeWeight(AExit);
        auto &EntryB = S.second.second;
        APInt Post   = Enc.AG.getEdgeWeight(EntryB);

        BasicBlock *N = interpose(Src, Tgt);

        // Since we always add instrumentation
        insertInc(N, Post, Ctr);
        insertLogPath(N, FuncId, Ctr, Zap);
        insertInc(N, Pre, Ctr);
    }

    // Add the logpath function for all function exiting
    // basic blocks.
    for (auto &EB : ExitBlocks) {
        insertLogPath(EB, FuncId, Ctr, Zap);
    }

    // Add the counter as the first instruction in the entry
    // block of the function. Set the counter to zero.
    Ctr->insertBefore(&*F.getEntryBlock().getFirstInsertionPt());
    auto *SI = new StoreInst(Zap, Ctr);
    SI->insertAfter(Ctr);

    // saveModule(*M, "test.bc");
}

char EPPProfile::ID = 0;
