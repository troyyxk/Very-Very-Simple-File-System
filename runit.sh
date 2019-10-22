#!/bin/bash

mkdir /tmp/mnt99
truncate -s 64M img
mkfs.a1fs -i 100 img
a1fs img /tmp/mnt99

cd /tmp/mnt99 || exit
mkdir dir_1
cd dir_1 || exit
mkdir dir_1_1
cd ..
rmdir dir_1/dir_1_1

touch f1
stat f1
touch dir_1/f1
unlink dir_1/f1

echo "testflight" >> f1
cat f1

stat f1
truncate -s 8192 f1
stat f1
truncate -s 4 f1
stat f1
