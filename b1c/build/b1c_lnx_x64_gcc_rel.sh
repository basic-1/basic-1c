#!/bin/bash

../../common/build/lnx_build.sh "../.." b1c b1c x64 gcc rel "-DCMAKE_BUILD_TYPE=RELEASE" $1
