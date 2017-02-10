
int main(int argc, char* argv[]) { return 0; }

// RUN: clang -c -g -emit-llvm %s -o %t.bc 
// RUN: llvm-epp -epp-fn=main %t.bc -o %t 2>&1 | tail -n 1 > %t1
// RUN: clang %t -o %t-exec
// RUN: ./%t-exec
