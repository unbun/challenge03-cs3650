#!/bin/bash
cd mnt
mkdir -p numbers

echo "===== writing 50 ====="

for i in {1..50}
do
   echo "$i" > numbers/$i.txt
done

echo "===== reading 50 ====="

for i in {1..50}
do
   cat numbers/$i.txt
done
