
int main(int argc, char* argv[]) { return 0; }

// RUN: clang -c -g -emit-llvm %s -o %T/%t.bc 
// RUN: llvm-epp %T/%t.bc -o %T/%t
// RUN: clang %T/%t.epp.bc -o %T/test-exec
// RUN: %T/text-exec 
