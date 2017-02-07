## llvm-epp 
Efficient Path Profiling using LLVM 

[![Build Status](https://travis-ci.org/sfu-arch/llvm-epp.svg?branch=master)](https://travis-ci.org/sfu-arch/llvm-epp)

## Requires 

1. LLVM 3.8
2. llvm-lit 
3. gcc-5

### From Source 

1.  
    wget http://llvm.org/releases/3.8.1/llvm-3.8.1.src.tar.xz \  
    http://llvm.org/releases/3.8.1/cfe-3.8.1.src.tar.xz \  
    && tar xf llvm-3.8.1.src.tar.xz && tar xf cfe-3.8.1.src.tar.xz \  
    && mv cfe-3.8.1.src llvm-3.8.1.src/tools/clang

2. `mkdir llvm-build && cd llvm-build`
3. `cmake -DCMAKE_BUILD_TYPE=Release -DLLVM_INSTALL_UTILS=ON ../llvm-3.8.1.src && make -j 8`
4. `sudo make install` 

## Build 

1. `mkdir build && cd build`
2. `cmake -DCMAKE_BUILD_TYPE=Release .. && make -j 8`
3. `sudo make install`

## Test

1. `lit ` 

## Usage

1. `clang -c -g -emit-llvm prog.c`
2. `llvm-epp prog.bc -o prog`
3. `clang prog.epp.bc -o exe`
4. `./exe`
5. `llvm-epp -p=path-profile-results.txt prog.bc`

## ToDo

1. Multiple function support
2. Multithread support


## License 

The MIT License

