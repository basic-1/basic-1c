#!/bin/bash

cd ./a1stm8/build
./a1stm8_lnx_x64_gcc_rel.sh $1
cd ../..

cd ./b1c/build
./b1c_lnx_x64_gcc_rel.sh $1
cd ../..

cd ./c1stm8/build
./c1stm8_lnx_x64_gcc_rel.sh $1
cd ../..