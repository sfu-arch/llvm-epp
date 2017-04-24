#define DEBUG_TYPE "epp_tool"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TypeBasedAliasAnalysis.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/LinkAllAsmWriterComponents.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Linker/Linker.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/Scalar.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/Passes.h"

#include <memory>
#include <string>

//#include "EPPDecode.h"
#include "EPPPathPrinter.h"
#include "EPPProfile.h"

using namespace std;
using namespace llvm;
using namespace llvm::sys;
using namespace epp;

cl::OptionCategory LLVMEppOptionCategory("EPP Options",
                                         "Additional options for the EPP tool");

cl::opt<string> inPath(cl::Positional, cl::desc("<Module to analyze>"),
                       cl::value_desc("bitcode filename"), cl::Required,
                       cl::cat(LLVMEppOptionCategory));

cl::opt<string>
    profileOutputFilename("o", cl::desc("Filename of the output path profile"),
                          cl::value_desc("filename"),
                          cl::cat(LLVMEppOptionCategory),
                          cl::init("path-profile-results.txt"));

cl::opt<string> profile("p", cl::desc("Path to path profiling results"),
                        cl::value_desc("filename"),
                        cl::cat(LLVMEppOptionCategory));

cl::opt<bool> wideCounter(
    "w",
    cl::desc("Use wide (128 bit) counters. Only available on 64 bit systems"),
    cl::value_desc("boolean"), cl::init(false), cl::cat(LLVMEppOptionCategory));


static void saveModule(Module &m, StringRef filename) {
    error_code EC;
    raw_fd_ostream out(filename.data(), EC, sys::fs::F_None);

    if (EC) {
        report_fatal_error("error saving llvm module to '" + filename +
                           "': \n" + EC.message());
    }
    WriteBitcodeToFile(&m, out);
}

static void instrumentModule(Module &module) {
    // Build up all of the passes that we want to run on the module.
    legacy::PassManager pm;
    pm.add(new llvm::AssumptionCacheTracker());
    pm.add(createLoopSimplifyPass());
    pm.add(llvm::createBasicAAWrapperPass());
    pm.add(createTypeBasedAAWrapperPass());
    pm.add(new llvm::CallGraphWrapperPass());
    pm.add(createBreakCriticalEdgesPass());
    pm.add(new LoopInfoWrapperPass());
    pm.add(new epp::EPPProfile());
    pm.add(createVerifierPass());
    pm.run(module);

    auto replaceExt = [](string &s, const string &newExt) {
        string::size_type i = s.rfind('.', s.length());
        if (i != string::npos) {
            s.replace(i + 1, newExt.length(), newExt);
        }
    };

    replaceExt(inPath, "epp.bc");
    saveModule(module, inPath);
}

static void interpretResults(Module &module, std::string filename) {

    legacy::PassManager pm;
    pm.add(new llvm::AssumptionCacheTracker());
    pm.add(createLoopSimplifyPass());
    pm.add(createBasicAAWrapperPass());
    pm.add(createTypeBasedAAWrapperPass());
    pm.add(new llvm::CallGraphWrapperPass());
    pm.add(createBreakCriticalEdgesPass());
    pm.add(new LoopInfoWrapperPass());
    pm.add(new epp::EPPDecode(filename));
    pm.add(new epp::EPPPathPrinter());
    pm.add(createVerifierPass());
    pm.run(module);
}

int main(int argc, char **argv, const char **env) {
    // This boilerplate provides convenient stack traces and clean LLVM exit
    // handling. It also initializes the built in support for convenient
    // command line option handling.
    sys::PrintStackTraceOnErrorSignal();
    llvm::PrettyStackTraceProgram X(argc, argv);
    LLVMContext &context = getGlobalContext();
    llvm_shutdown_obj shutdown;

    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmPrinters();
    InitializeAllAsmParsers();
    cl::AddExtraVersionPrinter(
        TargetRegistry::printRegisteredTargetsForVersion);
    cl::ParseCommandLineOptions(argc, argv);

    // Construct an IR file from the filename passed on the command line.
    SMDiagnostic err;
    unique_ptr<Module> module = parseIRFile(inPath.getValue(), err, context);

    if (!module.get()) {
        errs() << "Error reading bitcode file.\n";
        err.print(argv[0], errs());
        return -1;
    }

    if (!profile.empty()) {
        interpretResults(*module, profile.getValue());
    } else {
        instrumentModule(*module);
    }

    return 0;
}
