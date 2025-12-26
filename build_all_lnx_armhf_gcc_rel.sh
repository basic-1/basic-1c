#!/bin/bash

cd ./a1stm8/build
./a1stm8_lnx_armhf_gcc_rel.sh $1
cd ../..

cd ./b1c/build
./b1c_lnx_armhf_gcc_rel.sh $1
cd ../..

cd ./c1stm8/build
./c1stm8_lnx_armhf_gcc_rel.sh $1
cd ../..


cd ./a1rv32/build
./a1rv32_lnx_armhf_gcc_rel.sh $1
cd ../..