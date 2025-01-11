#!/bin/bash

#本文件用于创建testfile
cd /home/qnwang/worknew/AR/testclient/testfile
NAME_LISTS=(1k,5k,10k,20k,30k)
BS="1K" # 1M/1K
COUNT="1"
for NAME in "${NAME_LISTS[@]}"; do
    dd if=/dev/zero of=$NAME bs=1k count=1
done
dd if=/dev/zero of=1M bs=1M count=1
