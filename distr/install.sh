#!/bin/bash

if [ "$1" = "" ]
then
  echo "no argument"
  echo "usage: sudo ./install.sh <tar_gz_package_name>"
  exit 1
fi

#extract tar.gz file content to /tmp directory
tar -xzvf $1 -C /tmp

#copy the content to /usr directory:
cp -r /tmp/b1c/* /usr

#delete the extracted files
rm -r /tmp/b1c
