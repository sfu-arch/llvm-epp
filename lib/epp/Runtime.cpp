#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>
#include <cinttypes>

extern "C" {

// This macro allows us to prefix strings so that they are less likely to
// conflict with existing symbol names in the examined programs.
// e.g. EPP(entry) yields PaThPrOfIlInG_entry
#define EPP(X) PaThPrOfIlInG_##X

#ifdef __LP64__

std::vector<std::map<__int128, uint64_t>> EPP(path64);

void EPP(init)(uint32_t NumberOfFunctions) {
    EPP(path64).resize(NumberOfFunctions);
}

void EPP(logPath2)(__int128 Val, uint32_t FunctionId) {
    EPP(path64)[FunctionId][Val] += 1;
}

void EPP(save)(char *path) {
    // FILE *fp = fopen("path-profile-results.txt", "w");
    FILE *fp = fopen(path, "w");
    for (uint32_t I = 0; I < EPP(path64).size(); I++) {
        auto &FV = EPP(path64)[I];
        fprintf(fp, "%u %lu\n", I, FV.size());
        for (auto &KV : FV) {
            uint64_t low  = (uint64_t)KV.first;
            uint64_t high = (KV.first >> 64);
            // Print the hex values with a 0x prefix messes up
            // the APInt constructor.
            fprintf(fp, "%016" PRIx64 "%016" PRIx64 " %" PRIu64 "\n", high, low, KV.second);
        }
    }
    fclose(fp);
}

#endif

std::vector<std::map<uint64_t, uint64_t>> EPP(path32);

void EPP(init32)(uint32_t NumberOfFunctions) {
    EPP(path32).resize(NumberOfFunctions);
}

void EPP(logPath32)(uint64_t Val, uint32_t FunctionId) {
    EPP(path32)[FunctionId][Val] += 1;
}

// Update this similar to the 64 bit version
void EPP(save32)(char *path) {
    FILE *fp = fopen(path, "w");
    //for (auto &FV : EPP(path32)) {
    for (uint32_t I = 0; I < EPP(path32).size(); I++) {
        auto &FV = EPP(path32)[I];
        fprintf(fp, "%u %lu\n", I, FV.size());
        for (auto &KV : FV) {
            // Print the hex values with a 0x prefix messes up
            // the APInt constructor.
            fprintf(fp, "%016" PRIx64 " %" PRIu64 "\n", KV.first, KV.second);
        }
    }
    fclose(fp);
}

}
