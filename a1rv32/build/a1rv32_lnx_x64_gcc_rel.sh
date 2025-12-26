#!/bin/bash

../../common/build/lnx_build.sh "../.." a1rv32 a1rv32 x64 gcc rel "-DCMAKE_BUILD_TYPE=RELEASE" $1
