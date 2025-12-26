#!/bin/bash

../../common/build/lnx_build.sh "../.." a1rv32 a1rv32 armhf gcc rel "-DCMAKE_BUILD_TYPE=RELEASE" $1
