#include <cinttypes>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

using namespace std;

#define EPP(X) __epp_##X

extern uint32_t EPP(numberOfFunctions);

typedef vector<map<uint64_t, uint64_t>> TLSDataTy;
list<shared_ptr<TLSDataTy>> GlobalEPPDataList;

mutex tlsMutex;

class EPP(data) {
    // thread::id tid;
    shared_ptr<TLSDataTy> Ptr;

  public:
    void log(uint64_t Val, uint64_t FunctionId) {
        // cout << "log " << tid << " " << Val << " " << FunctionId << endl;
        (*Ptr)[FunctionId][Val] += 1;
    }

    EPP(data)() {
        // tid = this_thread::get_id();
        // cout << "ctor " << tid << endl;

        lock_guard<mutex> lock(tlsMutex);
        Ptr = make_shared<TLSDataTy>();
        GlobalEPPDataList.push_back(Ptr);
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

    // Save the data to a file

    for (uint32_t I = 0; I < Accumulate.size(); I++) {
        fprintf(fp, "%u %lu\n", I, Accumulate[I].size());
        for (auto &KV : Accumulate[I]) {
            fprintf(fp, "%016" PRIx64 " %" PRIu64 "\n", KV.first, KV.second);
        }
    }

    fclose(fp);
}
}
