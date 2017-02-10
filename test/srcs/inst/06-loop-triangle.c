
int main(int argc, char* argv[]) { 
    for(int i = 0; i < argc; i++) {
        if(i%2) {
            printf("This is a loop");
        }
    }
    return 0;
}

// RUN: clang -c -g -emit-llvm %s -o %t.bc 
// RUN: llvm-epp -epp-fn=main %t.bc -o %t 2>&1 | tail -n 1 > %t1
// RUN: echo "NumPaths : 4" > %t2
// RUN: diff %t1 %t2 
