

int main(int argc, char* argv[]) { 
    for(int i = 0; i < 10; i++) {
        switch(i%3) {
            case 0:
                printf("case 0");
                break;
            case 1:
                printf("case 1");
                break;
            default:
                printf("case ");
        }
    }
    return 0;
}

// RUN: clang -c -g -emit-llvm %s -o %t.1.bc 
// RUN: opt -instnamer %t.1.bc -o %t.bc
// RUN: llvm-epp %t.bc -o %t.profile
// RUN: clang -v %t.epp.bc -o %t-exec -lepp-rt 2> %t.compile 
// RUN: %t-exec > %t.log
// RUN: llvm-epp -p=%t.profile %t.bc 2> %t.decode
// RUN: diff -aub %t.profile %s.txt
