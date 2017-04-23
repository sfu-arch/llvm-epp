#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

extern "C" {

// This macro allows us to prefix strings so that they are less likely to
// conflict with existing symbol names in the examined programs.
// e.g. EPP(entry) yields PaThPrOfIlInG_entry
#define EPP(X) PaThPrOfIlInG_##X

// We only want to build the wide version of the runtime library, i.e.
// support for 128 bit counters on 64 bit machines, __int128t is not
// defined on 32 bit architectures.
#ifdef __LP64__

std::vector<std::map<__int128, uint64_t>> EPP(pathW);

void EPP(initW)(uint32_t NumberOfFunctions) {
    EPP(pathW).resize(NumberOfFunctions);
}

void EPP(logPathW)(__int128 Val, uint32_t FunctionId) {
    EPP(pathW)[FunctionId][Val] += 1;
}

void EPP(saveW)(char *path) {
    FILE *fp = fopen(path, "w");
    for (uint32_t I = 0; I < EPP(pathW).size(); I++) {
        auto &FV = EPP(pathW)[I];
        fprintf(fp, "%u %lu\n", I, FV.size());
        for (auto &KV : FV) {
            uint64_t low  = (uint64_t)KV.first;
            uint64_t high = (KV.first >> 64);
            // Print the hex values with a 0x prefix messes up
            // the APInt constructor in the decoder
            fprintf(fp, "%016" PRIx64 "%016" PRIx64 " %" PRIu64 "\n", high, low,
                    KV.second);
        }
    }
    fclose(fp);
}

#endif

std::vector<std::map<uint64_t, uint64_t>> EPP(path);

void EPP(init)(uint32_t NumberOfFunctions) {
    EPP(path).resize(NumberOfFunctions);
}

void EPP(logPath)(uint64_t Val, uint32_t FunctionId) {
    EPP(path)[FunctionId][Val] += 1;
}

void EPP(save)(char *path) {
    FILE *fp = fopen(path, "w");
    for (uint32_t I = 0; I < EPP(path).size(); I++) {
        auto &FV = EPP(path)[I];
        fprintf(fp, "%u %lu\n", I, FV.size());
        for (auto &KV : FV) {
            // Print the hex values with a 0x prefix messes up
            // the APInt constructor in the decoder
            fprintf(fp, "%016" PRIx64 " %" PRIu64 "\n", KV.first, KV.second);
        }
    }
    fclose(fp);
}
}
