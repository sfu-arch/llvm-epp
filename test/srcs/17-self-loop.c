
int main(int argc, char* argv[]) { 
loop:
    printf("loop\n");
    goto loop;
    return 0;
}

// RUN: clang -O3 -c -g -emit-llvm %s -o %t.1.bc 
// RUN: opt -instnamer %t.1.bc -o %t.bc
// RUN: llvm-epp %t.bc -o %t.profile
// RUN: clang -v %t.epp.bc -o %t-exec -lepp-rt 2> %t.compile 
// RUN: timeout 0.01s %t-exec || true
