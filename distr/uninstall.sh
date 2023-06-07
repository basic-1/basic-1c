#!/bin/bash

project_name=b1c

#delete binaries
rm /usr/local/bin/b1c
rm /usr/local/bin/c1stm8
rm /usr/local/bin/a1stm8

#delete docs
rm -r /usr/local/share/doc/$project_name

#delete libraries
rm -r /usr/local/share/$project_name