#include <cstdio>
#include <iostream>

// This tests for instrumentation along critical edges where
// the destination of the edge is an exception handling pad block.

int main(int argc, char* argv[]) { 
    try {
        std::cout << "test";
        std::cout << "test2";
    } catch(...) {
        printf("Caught an exception.");
    }
    return 0; 
}

// RUN: clang -c -g -emit-llvm %s -o %t.1.bc 
// RUN: opt -O2 -instnamer %t.1.bc -o %t.bc
// RUN: llvm-epp %t.bc -o %t.profile 2> %t.epp.log
// RUN: clang -v %t.epp.bc -o %t-exec -lepp-rt -lstdc++ 2> %t.compile 
// RUN: %t-exec > %t.log
// RUN: llvm-epp -p=%t.profile %t.bc 2> %t.decode
// RUN: diff -aub %t.profile %s.txt
