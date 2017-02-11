
int main(int argc, char* argv[]) { 
    if(argc > 2) {
        printf("This is a triangle");
    } 
    else {
        printf("This is a diamond");
    }
    if(argc > 4) {
        printf("This is a triangle x2");
    } 
    else {
        printf("This is a diamond x2");
    }
    return 0; 
}

// RUN: clang -c -g -emit-llvm %s -o %t.bc 
// RUN: llvm-epp %t.bc -o %t 
// RUN: clang -v %t.epp.bc -o %t-exec -lepp-rt 2> %t.compile 
// RUN: %t-exec > %t.log
// RUN: llvm-epp -p=path-profile-results.txt %t.bc 2> %t.decode
