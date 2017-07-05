#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>
#include <thread>
#include <mutex>


extern uint32_t __epp_numberOfFunctions;

extern "C" {

// This macro allows us to prefix strings so that they are less likely to
// conflict with existing symbol names in the examined programs.
// e.g. EPP(entry) yields PaThPrOfIlInG_entry
#define EPP(X) PaThPrOfIlInG_##X

std::vector<std::map<uint64_t, uint64_t>> EPP(path_g);
std::mutex tlsMutex;

class EPP(data) {
    std::vector<std::map<uint64_t, uint64_t>> path;
    std::thread::id tid;

public:
    void log(uint64_t Val, uint32_t FunctionId) {
        path[FunctionId][Val] += 1;
    }

    EPP(data)() {
        tid = std::this_thread::get_id();
        path.resize(__epp_numberOfFunctions);
    }

    ~EPP(data)() {
        std::lock_guard<std::mutex> lock(tlsMutex);
        for (uint32_t I = 0; I < path.size(); I++) {
            auto &FV = path[I];
            for (auto &KV : FV) {
                EPP(path_g)[I][KV.first] += KV.second;
            }
        }
    }
};

thread_local EPP(data) Data;

void EPP(init)() {
    EPP(path_g).resize(__epp_numberOfFunctions);
}

void EPP(logPath)(uint64_t Val, uint32_t FunctionId) {
    Data.log(Val, FunctionId);
}

void EPP(save)(char *path) {
    FILE *fp = fopen(path, "w");
    for (uint32_t I = 0; I < EPP(path_g).size(); I++) {
        auto &FV = EPP(path_g)[I];
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
