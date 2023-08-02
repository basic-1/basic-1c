#!/bin/bash

#usage: ./lnx_tar_gz_distr.sh <project_name> <platform> <lnx_arch> <compiler> [<next>]
#<platform>=x86|x64|armhf
#<lnx_arch>=i386|amd64|armhf
#<compiler>=gcc

if [ "$1" = "" ] || [ "$2" = "" ] || [ "$3" = "" ] || [ "$4" = "" ]
then
  echo "invalid arguments"
  exit 1
fi

#temporary directory to build package in
#tmp_dir=./
tmp_dir=/tmp/

#install directory name
proj_dir_name=b1c

#get command line arguments
project_name=$1
platform=$2
lnx_arch=$3
compiler=$4

#get interpreter version
if [ -f "../../common/source/version.h" ]
then
  version=$(cat "../../common/source/version.h" | tr -d '\r' | tr -d '\n')
  if [ "$version" = "" ]
  then
    echo "get version error"
    exit 1
  fi
else
  echo "get version error"
  exit 1
fi
version=${version/"#define"/}
version=${version/"B1_CMP_VERSION"/}
version=${version// /}
version=${version//'"'/}

#get build number
if [ -f "../../common/source/gitrev.h" ]
then
  build_num=$(cat "../../common/source/gitrev.h" | tr -d '\r' | tr -d '\n')
  if [ "$build_num" = "" ]
  then
    echo "no build number found"
  else
    build_num=${build_num/"#define"/}
    build_num=${build_num/"B1_GIT_REVISION"/}
    build_num=${build_num// /}
    build_num=${build_num//'"'/}

#do not increase current revision number from gitrev.h
#    if [ "$5" = "next" ]
#    then
#      ((build_num++))
#    fi
  fi
else
  echo "no build number found"
fi

#create directories for tar.gz package
mkdir -p -m 775 $tmp_dir$proj_dir_name
mkdir -p -m 775 $tmp_dir$proj_dir_name/local
mkdir -p -m 775 $tmp_dir$proj_dir_name/local/bin
mkdir -p -m 775 $tmp_dir$proj_dir_name/local/share
mkdir -p -m 775 $tmp_dir$proj_dir_name/local/share/$proj_dir_name
mkdir -p -m 775 $tmp_dir$proj_dir_name/local/share/doc
mkdir -p -m 775 $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name
mkdir -p -m 775 $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name/b1core
mkdir -p -m 775 $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name/b1core/docs
mkdir -p -m 775 $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name/b1c
mkdir -p -m 775 $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name/c1stm8
mkdir -p -m 775 $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name/a1stm8

#copy executable modules, samples, docs and README
cp ../../bin/lnx/$platform/$compiler/rel/b1c $tmp_dir$proj_dir_name/local/bin/
cp ../../bin/lnx/$platform/$compiler/rel/c1stm8 $tmp_dir$proj_dir_name/local/bin/
cp ../../bin/lnx/$platform/$compiler/rel/a1stm8 $tmp_dir$proj_dir_name/local/bin/

cp ../../b1core/docs/changelog $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name/b1core/docs
cp ../../b1core/LICENSE $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name/b1core
cp ../../b1core/README.md $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name/b1core

cp -af ../../b1c/docs $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name/b1c
cp -af ../../c1stm8/docs $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name/c1stm8
cp -af ../../a1stm8/docs $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name/a1stm8

cp ../../README.md $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name
cp ../../LICENSE $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name
cp ../../common/docs/changelog $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name

cp -af ../../common/lib $tmp_dir$proj_dir_name/local/share/$proj_dir_name

#strip copied executables
strip --strip-all $tmp_dir$proj_dir_name/local/bin/b1c
strip --strip-all $tmp_dir$proj_dir_name/local/bin/c1stm8
strip --strip-all $tmp_dir$proj_dir_name/local/bin/a1stm8

#correct file attributes
chmod 755 `find $tmp_dir$proj_dir_name -type d`
chmod 755 $tmp_dir$proj_dir_name/local/bin/*
chmod 644 `find $tmp_dir$proj_dir_name/local/share/$proj_dir_name -type f`
chmod 644 `find $tmp_dir$proj_dir_name/local/share/doc/$proj_dir_name -type f`

orig_path=`pwd`
cd $tmp_dir

#create tar.gz package
tar -zcvf $project_name.tar.gz $proj_dir_name

#delete all the stuff we collected for the package
rm -r $proj_dir_name

cd $orig_path

#delete package with the same name from distr directory
if [ "$build_num" = "" ]
then
  rm ../../distr/${project_name}_lnx_${lnx_arch}_${compiler}_${version}.tar.gz
else
  rm ../../distr/${project_name}_lnx_${lnx_arch}_${compiler}_${version}-*.tar.gz
fi

#move the package to distr directory
if [ "$build_num" = "" ]
then
  mv $tmp_dir$project_name.tar.gz ../../distr/${project_name}_lnx_${lnx_arch}_${compiler}_${version}.tar.gz
else
  mv $tmp_dir$project_name.tar.gz ../../distr/${project_name}_lnx_${lnx_arch}_${compiler}_${version}-${build_num}.tar.gz
fi