
int main(int argc, char* argv[]) { 
    if(argc > 2) {
        printf("This is a triangle");
    }
    return 0; 
}

// RUN: clang -c -g -emit-llvm %s -o %t.bc 
// RUN: llvm-epp %t.bc -o %t 2>&1 | tail -n 1 > %t1
// RUN: echo "NumPaths : 2" > %t2
// RUN: diff %t1 %t2 
