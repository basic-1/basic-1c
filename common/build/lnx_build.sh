#!/bin/bash
# <root_path> <target_name> <project_name> <platform> <compiler> <configuration> <cmake1_options> [<is_next_revision>]

if [ "$1" = "" ] || [ "$2" = "" ] || [ "$3" = "" ] || [ "$4" = "" ] || [ "$5" = "" ] || [ "$6" = "" ] || [ "$7" = "" ]
then
  echo "invalid arguments"
  exit 1
fi

target_name=$2
project_name=$3
platform=$4
compiler=$5
configuration=$6

mkdir $1/build
mkdir $1/build/lnx
mkdir $1/build/lnx/$platform
mkdir $1/build/lnx/$platform/$compiler
mkdir $1/build/lnx/$platform/$compiler/$configuration
mkdir $1/build/lnx/$platform/$compiler/$configuration/$target_name

mkdir $1/bin
mkdir $1/bin/lnx
mkdir $1/bin/lnx/$platform
mkdir $1/bin/lnx/$platform/$compiler
mkdir $1/bin/lnx/$platform/$compiler/$configuration

env_dir=$1/env
combld_dir=$1/common/build
comsrc_dir=$1/common/source

if [ -f "${combld_dir}/get_git_rev.sh" ]
then
  . ${combld_dir}/get_git_rev.sh "${comsrc_dir}" "$8"
fi

if [ -f "${env_dir}/lnx_${platform}_${compiler}_env.sh" ]
then
  . ${env_dir}/lnx_${platform}_${compiler}_env.sh
  export B1_WX_CONFIG
fi

B1_TARGET=$target_name
export B1_TARGET

cd $1/build/lnx/$platform/$compiler/$configuration/$target_name

cmake ../../../../../../$project_name/source $7
cmake --build . --target $target_name

cp $target_name ../../../../../../bin/lnx/$platform/$compiler/$configuration

cd ../../../../../../$project_name/build
