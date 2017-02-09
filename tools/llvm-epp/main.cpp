#define DEBUG_TYPE "epp_tool"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
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
#include "llvm/Analysis/CallGraph.h"
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

//#include "AllInliner.h"
#include "Common.h"
#include "EPPDecode.h"
#include "EPPProfile.h"
//#include "Namer.h"
//#include "Simplify.h"

#include "config.h"

using namespace std;
using namespace llvm;
using namespace llvm::sys;
using namespace epp;

cl::OptionCategory LLVMEppOptionCategory("EPP Options","Additional options for the EPP tool");

cl::opt<string> inPath(cl::Positional, cl::desc("<Module to analyze>"),
                       cl::value_desc("bitcode filename"), cl::Required, cl::cat(LLVMEppOptionCategory));

cl::opt<string> outFile("o", cl::desc("Filename of the instrumented bitcode"),
                        cl::value_desc("filename"), cl::cat(LLVMEppOptionCategory));

cl::opt<string> profile("p", cl::desc("Path to path profiling results"),
                        cl::value_desc("filename"), cl::cat(LLVMEppOptionCategory));

//cl::opt<unsigned>
    //numberOfPaths("n", cl::desc("Number of most frequent paths to compute"),
                  //cl::value_desc("number"), cl::init(5));

// Determine optimization level.
//cl::opt<char> optLevel("O",
                       //cl::desc("Optimization level. [-O0, -O1, -O2, or -O3] "
                                //"(default = '-O2')"),
                       //cl::Prefix, cl::ZeroOrMore, cl::init('2'));

//cl::list<string> libPaths("L", cl::Prefix,
                          //cl::desc("Specify a library search path"),
                          //cl::value_desc("directory"));

//cl::list<string> libraries("l", cl::Prefix,
                           //cl::desc("Specify libraries to link to"),
                           //cl::value_desc("library prefix"));


cl::list<std::string> FunctionList("epp-fn", cl::value_desc("String"),
                                   cl::desc("List of functions to instrument"),
                                   cl::OneOrMore, cl::CommaSeparated);

cl::opt<bool> printSrcLines("src", cl::desc("Print Source Line Numbers"),
                            cl::init(false));

bool isTargetFunction(const Function &f,
                      const cl::list<std::string> &FunctionList) {
    if (f.isDeclaration())
        return false;
    for (auto &fname : FunctionList)
        if (fname == f.getName())
            return true;
    return false;
}

static
void saveModule(Module &m, StringRef filename) {
    error_code EC;
    raw_fd_ostream out(filename.data(), EC, sys::fs::F_None);

    if (EC) {
        report_fatal_error("error saving llvm module to '" + filename +
                           "': \n" + EC.message());
    }
    WriteBitcodeToFile(&m, out);
}


static void instrumentModule(Module &module, std::string outFile,
                             const char *argv0) {
    // Build up all of the passes that we want to run on the module.
    legacy::PassManager pm;
    // pm.add(new DataLayoutWrapperPass());
    pm.add(new llvm::AssumptionCacheTracker());
    pm.add(createLoopSimplifyPass());
    pm.add(llvm::createBasicAAWrapperPass());
    pm.add(createTypeBasedAAWrapperPass());
    pm.add(new llvm::CallGraphWrapperPass());
    //pm.add(new epp::PeruseInliner());
    //pm.add(new pasha::Simplify(FunctionList[0]));
    pm.add(createBreakCriticalEdgesPass());
    //pm.add(new epp::Namer());
    pm.add(new LoopInfoWrapperPass());
    pm.add(new epp::EPPProfile());
    pm.add(createVerifierPass());
    pm.run(module);

    // First search the directory of the binary for the library, in case it is
    // all bundled together.
    //SmallString<32> invocationPath(argv0);
    //sys::path::remove_filename(invocationPath);
    //if (!invocationPath.empty()) {
        //libPaths.push_back(invocationPath.str());
    //}
//// If the builder doesn't plan on installing it, we still need to get to the
//// runtime library somehow, so just build in the path to the temporary one.
//#ifdef CMAKE_INSTALL_PREFIX
    //libPaths.push_back(CMAKE_INSTALL_PREFIX "/lib");
//#elif defined(CMAKE_TEMP_LIBRARY_PATH)
    //libPaths.push_back(CMAKE_TEMP_LIBRARY_PATH);
//#endif
    //libraries.push_back(RUNTIME_LIB);
    //libraries.push_back("rt");
    //libraries.push_back("m");

    saveModule(module, outFile + ".epp.bc");
    //common::generateBinary(module, outFile, optLevel, libPaths, libraries);
}

static void interpretResults(Module &module, std::string filename) {

    legacy::PassManager pm;
    // pm.add(new DataLayoutPass());
    pm.add(new llvm::AssumptionCacheTracker());
    pm.add(createLoopSimplifyPass());
    pm.add(createBasicAAWrapperPass());
    pm.add(createTypeBasedAAWrapperPass());
    pm.add(new llvm::CallGraphWrapperPass());
    //pm.add(new epp::PeruseInliner());
    //pm.add(new pasha::Simplify(FunctionList[0]));
    pm.add(createBreakCriticalEdgesPass());
    //pm.add(new epp::Namer());
    pm.add(new LoopInfoWrapperPass());
    pm.add(new epp::EPPDecode());
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

    //common::optimizeModule(module.get());

    if (!profile.empty()) {
        interpretResults(*module, profile);
    } else if (!outFile.empty()) {
        instrumentModule(*module, outFile, argv[0]);
    } else {
        errs() << "Neither -o nor -p were selected!\n";
        return -1;
    }

    return 0;
}
