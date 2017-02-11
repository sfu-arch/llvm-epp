
int main(int argc, char* argv[]) { 
    for(int i = 0; i < argc; i++) {
        if(i%2) {
            printf("This is a loop");
        }
    }
    return 0;
}

// RUN: clang -c -g -emit-llvm %s -o %t.bc 
// RUN: llvm-epp %t.bc -o %t 
// RUN: clang -v %t.epp.bc -o %t-exec -lepp-rt 2> %t.compile 
// RUN: %t-exec > %t.log
