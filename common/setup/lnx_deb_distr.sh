#!/bin/bash

#usage: ./lnx_deb_distr.sh <project_name> <platform> <lnx_arch> <compiler> <depends> [<next>]
#<project_name>=b1c
#<platform>=x86|x64|armhf
#<lnx_arch>=i386|amd64|armhf
#<compiler>=gcc

if [ "$1" = "" ] || [ "$2" = "" ] || [ "$3" = "" ] || [ "$4" = "" ] || [ "$5" = "" ]
then
  echo "invalid arguments"
  exit 1
fi

#temporary directory to build package in
#tmp_dir=./
tmp_dir=/tmp/

proj_dir_name=b1c

#get command line arguments
project_name=$1
platform=$2
lnx_arch=$3
compiler=$4
depends=$5

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
    echo "get build number error"
  else
    build_num=${build_num/"#define"/}
    build_num=${build_num/"B1_GIT_REVISION"/}
    build_num=${build_num// /}
    build_num=${build_num//'"'/}

#do not increase current revision number from gitrev.h
#    if [ "$6" = "next" ]
#    then
#      ((build_num++))
#    fi
  fi
else
  echo "get build number error"
fi

#create directories for deb package
mkdir -p -m 775 $tmp_dir$proj_dir_name
mkdir -p -m 775 $tmp_dir$proj_dir_name/DEBIAN
mkdir -p -m 775 $tmp_dir$proj_dir_name/usr
mkdir -p -m 775 $tmp_dir$proj_dir_name/usr/bin
mkdir -p -m 775 $tmp_dir$proj_dir_name/usr/share
mkdir -p -m 775 $tmp_dir$proj_dir_name/usr/share/$proj_dir_name
mkdir -p -m 775 $tmp_dir$proj_dir_name/usr/share/doc
mkdir -p -m 775 $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name
mkdir -p -m 775 $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name/b1c
mkdir -p -m 775 $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name/c1stm8
mkdir -p -m 775 $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name/a1stm8

#copy executable modules, samples, docs and README
cp ../../bin/lnx/$platform/$compiler/rel/b1c $tmp_dir$proj_dir_name/usr/bin
cp ../../bin/lnx/$platform/$compiler/rel/c1stm8 $tmp_dir$proj_dir_name/usr/bin
cp ../../bin/lnx/$platform/$compiler/rel/a1stm8 $tmp_dir$proj_dir_name/usr/bin

cp -af ../../b1c/docs $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name/b1c
cp -af ../../c1stm8/docs $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name/c1stm8
cp -af ../../a1stm8/docs $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name/a1stm8

cp ../../README.md $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name

cp -af ../../common/lib $tmp_dir$proj_dir_name/usr/share/$proj_dir_name

#create deb package control file
echo "Package: $proj_dir_name" >$tmp_dir$proj_dir_name/DEBIAN/control
if [ "$build_num" = "" ]
then
  echo "Version: $version" >>$tmp_dir$proj_dir_name/DEBIAN/control
else
  echo "Version: $version-$build_num" >>$tmp_dir$proj_dir_name/DEBIAN/control
fi
echo "Maintainer: Nikolay Pletnev <b1justomore@gmail.com>" >>$tmp_dir$proj_dir_name/DEBIAN/control
echo "Architecture: $lnx_arch" >>$tmp_dir$proj_dir_name/DEBIAN/control
echo "Section: misc" >>$tmp_dir$proj_dir_name/DEBIAN/control
echo "Depends: $depends" >>$tmp_dir$proj_dir_name/DEBIAN/control
echo "Priority: optional" >>$tmp_dir$proj_dir_name/DEBIAN/control
echo "Description: STM8 BASIC1 compiler" >>$tmp_dir$proj_dir_name/DEBIAN/control
echo " Just one more BASIC compiler!" >>$tmp_dir$proj_dir_name/DEBIAN/control
echo " Type b1c to get usage help" >>$tmp_dir$proj_dir_name/DEBIAN/control

#copy license and changelog, compress changelog with gzip
cp ../../LICENSE $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name/copyright
cp ../../common/docs/changelog $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name/
gzip -9 -n -S.gz $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name/changelog
cp $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name/changelog.gz $tmp_dir$proj_dir_name/usr/share/doc/$proj_dir_name/changelog.Debian.gz

#strip copied executables
strip --strip-all $tmp_dir$proj_dir_name/usr/bin/b1c
strip --strip-all $tmp_dir$proj_dir_name/usr/bin/c1stm8
strip --strip-all $tmp_dir$proj_dir_name/usr/bin/a1stm8

#correct file attributes
chmod 755 `find $tmp_dir$proj_dir_name -type d`
chmod 755 $tmp_dir$proj_dir_name/usr/bin/*
chmod 644 `find $tmp_dir$proj_dir_name/usr/share -type f`

#create deb package
fakeroot dpkg-deb --build $tmp_dir$proj_dir_name

#delete all the stuff we collected for the package
rm -r $tmp_dir$proj_dir_name

#delete package with the same name from distr directory
if [ "$build_num" = "" ]
then
  rm ../../distr/${project_name}_lnx_${lnx_arch}_${compiler}_${version}.deb
else
  rm ../../distr/${project_name}_lnx_${lnx_arch}_${compiler}_${version}-*.deb
fi

#move the package to distr directory
if [ "$build_num" = "" ]
then
  mv $tmp_dir$proj_dir_name.deb ../../distr/${project_name}_lnx_${lnx_arch}_${compiler}_${version}.deb
else
  mv $tmp_dir$proj_dir_name.deb ../../distr/${project_name}_lnx_${lnx_arch}_${compiler}_${version}-${build_num}.deb
fi
