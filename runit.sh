#!/bin/bash

make all

rm /tmp/mnt99 -rf
mkdir /tmp/mnt99
rm img
truncate -s 64M img
mkfs.a1fs -i 100 img
a1fs img /tmp/mnt99

mkdir /tmp/mnt99/dir_1
mkdir /tmp/mnt99/dir_1/dir_1_1
rmdir /tmp/mnt99/dir_1/dir_1_1

touch /tmp/mnt99/f1
stat /tmp/mnt99/f1
touch /tmp/mnt99/dir_1/f2
unlink /tmp/mnt99/dir_1/f2

echo "testflight" >> /tmp/mnt99/f1
cat /tmp/mnt99/f1

touch /tmp/mnt99/f3
stat /tmp/mnt99/f3
truncate -s 8192 /tmp/mnt99/f3
stat /tmp/mnt99/f3
truncate -s 4 /tmp/mnt99/f3
stat /tmp/mnt99/f3

fusermount -u /tmp/mnt99/
a1fs img /tmp/mnt99
stat /tmp/mnt99/f1
