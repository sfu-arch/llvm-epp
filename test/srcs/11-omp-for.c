#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include <math.h>
int main()
{
    double sinTable[256];

#pragma omp parallel for
    for(int n=0; n<256; ++n)
        sinTable[n] = sin(2 * M_PI * n / 256);

    // the table is now initialized
}

// RUN: clang -fopenmp -c -g -emit-llvm %s -o %t.1.bc 
// RUN: opt -instnamer %t.1.bc -o %t.bc
// RUN: llvm-epp %t.bc -o %t.profile
// RUN: clang -fopenmp -v %t.epp.bc -o %t-exec -lepp-rt -lpthread -lm 2> %t.compile 
// RUN: OMP_NUM_THREADS=4 %t-exec > %t.log
// RUN: llvm-epp -p=%t.profile %t.bc 2> %t.decode
// RUN: diff -aub %t.profile %s.txt
