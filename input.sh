#!/bin/bash

number=$1

for ((i=1; i<=number; i++))
do
    echo $i >> numbers.txt
done