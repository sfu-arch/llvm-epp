#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

extern "C" {

// This macro allows us to prefix strings so that they are less likely to
// conflict with existing symbol names in the examined programs.
// e.g. EPP(entry) yields PaThPrOfIlInG_entry
#define EPP(X) PaThPrOfIlInG_##X

#ifndef RT32

std::vector<std::map<__int128, uint64_t>> EPP(path);

void EPP(init)(uint32_t NumberOfFunctions) {
    EPP(path).resize(NumberOfFunctions);
}

void EPP(logPath2)(__int128 Val, uint32_t FunctionId) { 
    EPP(path)[FunctionId][Val] += 1; 
}

void EPP(save)() {
    FILE *fp = fopen("path-profile-results.txt", "w");
    for (auto &FV : EPP(path)) {
        fprintf(fp, "%lu\n", FV.size());
        for (auto &KV : FV) {
            uint64_t low  = (uint64_t)KV.first;
            uint64_t high = (KV.first >> 64);
            // Print the hex values with a 0x prefix messes up
            // the APInt constructor.
            fprintf(fp, "%016lx%016lx %lu\n", high, low, KV.second);
        }
    }
    fclose(fp);
}

#else


std::vector<std::map<uint64_t, uint64_t>> EPP(path);

void EPP(init)(uint32_t NumberOfFunctions) {
    EPP(path).resize(NumberOfFunctions);
}

void EPP(logPath2)(uint64_t Val, uint32_t FunctionId) { 
    EPP(path)[FunctionId][Val] += 1; 
}

void EPP(save)() {
    FILE *fp = fopen("path-profile-results.txt", "w");
    for (auto &FV : EPP(path)) {
        fprintf(fp, "%u\n", EPP(path).size());
        for (auto &KV : FV) {
            // Print the hex values with a 0x prefix messes up
            // the APInt constructor.
            fprintf(fp, "%016llx %llu\n", KV.first, KV.second);
        }
    }
    fclose(fp);
}
#endif
}
