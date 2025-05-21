#!/usr/bin/bash

FILES=("
    https://raw.githubusercontent.com/richgel999/fpng/refs/heads/main/src/fpng.h
    https://raw.githubusercontent.com/richgel999/fpng/refs/heads/main/src/fpng.cpp
")

for file in $FILES
do
    wget $file
done
