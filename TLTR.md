# Very simple instruction

## To compile your pass
```
cd DynamicAnalysisPass/ 
mkdir build
cd build
cmake ..
make
cd ..
```

## To compile your Cuda program with pass
Take `gaussian` as an example
```
cd Results/gaussian/

clang++ -include ../../dynamicAnalysisCode.cu -g -flegacy-pass-manager -Xclang -load -Xclang ../../DynamicAnalysisPass/build/DynamicAnalysis/libDynamicAnalysisPass.so gaussian.cu -o gaussian --cuda-gpu-arch=sm_61 -L/usr/local/cuda-11.8/lib64/ -lcudart_static -ldl -lrt -pthread
```
The Pass will be registered only if you keep the flag `-flegacy-pass-manager`

## To see IR after code insertion

```
cd Results/gaussian/

clang++ -include ../../dynamicAnalysisCode.cu -g -flegacy-pass-manager -Xclang -load -Xclang ../../DynamicAnalysisPass/build/DynamicAnalysis/libDynamicAnalysisPass.so -std=c++11 -emit-llvm -c -S gaussian.cu --cuda-gpu-arch=sm_61 -L/usr/local/cuda-11.8/lib64/
```

## To run your program with analysis output
```
cd Results/gaussian/
./gaussian -s 16
```
or 
```
./Results/gaussian/gaussian -f Results/gaussian/data/matrix208.txt -s 16 | grep "DA__" | cat header.txt /dev/stdin | python3 computeStatistics.py /dev/stdin /dev/stdout
```