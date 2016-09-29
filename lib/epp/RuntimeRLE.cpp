#include <cstdint>
#include <cstdio>
#include <map>

extern "C" {

// This macro allows us to prefix strings so that they are less likely to
// conflict with existing symbol names in the examined programs.
// e.g. EPP(entry) yields PaThPrOfIlInG_entry
#define EPP(X) PaThPrOfIlInG_##X

#ifndef RT32

__int128 EPP(PathId);
uint64_t EPP(Counter);
FILE *fp = nullptr;

void EPP(init)() {
    EPP(PathId) = -1;
    fp          = fopen("path-profile-trace.txt", "w");
}

void EPP(logPath2)(__int128 Val) {
    if (EPP(PathId) == -1) {
        EPP(PathId)  = Val;
        EPP(Counter) = 1;
    } else if (EPP(PathId) == Val) {
        EPP(Counter) += 1;
    } else {
        uint64_t low  = (uint64_t)EPP(PathId);
        uint64_t high = (EPP(PathId) >> 64);
        fprintf(fp, "%016lx%016lx %lu\n", high, low, EPP(Counter));
        EPP(PathId)  = Val;
        EPP(Counter) = 1;
    }
}

void EPP(save)() { fclose(fp); }

#else

int64_t EPP(PathId);
uint64_t EPP(Counter);
FILE *fp = nullptr;

void EPP(init)() {
    EPP(PathId) = -1;
    fp          = fopen("path-profile-trace.txt", "w");
}

void EPP(logPath2)(uint64_t Val) {
    if (EPP(PathId) == -1) {
        EPP(PathId)  = Val;
        EPP(Counter) = 1;
    } else if (EPP(PathId) == Val) {
        EPP(Counter) += 1;
    } else {
        fprintf(fp, "%016llx %llu\n", EPP(PathId), EPP(Counter));
        EPP(PathId)  = Val;
        EPP(Counter) = 1;
    }
}

void EPP(save)() { fclose(fp); }
#endif
}
