#!/bin/bash

unlink $1/gitrev.h
touch $1/gitrev.h

if git_rev=`git rev-list --count HEAD`
then
  if [ "$2" == "next" ]
  then
    ((git_rev++))
  fi
  echo '#define B1_GIT_REVISION "'$git_rev'"' >>$1/gitrev.h
fi

