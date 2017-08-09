#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std;

#define EPP(X) __epp_##X

extern uint32_t EPP(numberOfFunctions);

typedef vector<unordered_map<uint64_t, uint64_t>> TLSDataTy;
list<shared_ptr<TLSDataTy>> GlobalEPPDataList;

mutex tlsMutex;

class EPP(data) {
    shared_ptr<TLSDataTy> Ptr;

  public:
    void log(uint64_t Val, uint64_t FunctionId) {
        // cout << "log " << tid << " " << Val << " " << FunctionId << endl;
        (*Ptr)[FunctionId][Val] += 1;
    }

    EPP(data)() {
        lock_guard<mutex> lock(tlsMutex);
        Ptr = make_shared<TLSDataTy>();
        GlobalEPPDataList.push_back(Ptr);
        // Allocate an unordered_map for each function even though we know it
        // may not
        // be used. This is to make the lookup faster at runtime.
        Ptr->resize(EPP(numberOfFunctions));
    }
};

/// Why is this unique_ptr?
///

thread_local unique_ptr<EPP(data)> Data = make_unique<EPP(data)>();

extern "C" {

void EPP(init)() {}

void EPP(logPath)(uint64_t Val, uint64_t FunctionId) {
    if (Data)
        Data->log(Val, FunctionId);
}

void EPP(save)(char *path) {

    FILE *fp = fopen(path, "w");

    // TODO: Modify to enable option of per thread dump

    TLSDataTy Accumulate(EPP(numberOfFunctions));

    for (auto T : GlobalEPPDataList) {
        for (uint32_t I = 0; I < T->size(); I++) {
            for (auto &KV : T->at(I)) {
                Accumulate[I][KV.first] += KV.second;
            }
        }
    }

    // Save the data to a file. Make the dump deterministic by
    // sorting the function ids, and then sorting the paths by
    // their freq/id. The path printer already sorts by freq.

    for (uint32_t I = 0; I < Accumulate.size(); I++) {
        if (Accumulate[I].size() > 0) {
            fprintf(fp, "%u %lu\n", I, Accumulate[I].size());
            vector<pair<uint64_t, uint64_t>> Values(Accumulate[I].begin(),
                                                    Accumulate[I].end());
            sort(Values.begin(), Values.end(),
                 [](const pair<uint64_t, uint64_t> &P1,
                    const pair<uint64_t, uint64_t> &P2) {
                     return (P1.second > P2.second) ||
                            (P1.second == P2.second && P1.first > P2.first);
                 });
            for (auto &KV : Values) {
                fprintf(fp, "%016" PRIx64 " %" PRIu64 "\n", KV.first,
                        KV.second);
            }
        }
    }

    fclose(fp);
}
}
