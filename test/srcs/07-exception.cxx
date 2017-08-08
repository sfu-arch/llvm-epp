#include <cstdio>


double foo(int x) {
    if(x == 2) {
        throw "Exception";
    }
    return 1*x;
}

int main(int argc, char* argv[]) { 
    try {
        foo(argc);
    } catch(...) {
        printf("Caught an exception.");
    }
    return 0; 
}

// RUN: clang -c -g -emit-llvm %s -o %t.1.bc 
// RUN: opt -instnamer %t.1.bc -o %t.bc
// RUN: llvm-epp %t.bc -o %t.profile 2> %t.epp.log
// RUN: clang -v %t.epp.bc -o %t-exec -lepp-rt -lstdc++ 2> %t.compile 
// RUN: %t-exec 2 > %t.log
// RUN: llvm-epp -p=%t.profile %t.bc 2> %t.decode
// RUN: diff -aub %t.profile %s.txt  
