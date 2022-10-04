#!/bin/bash

set -ex
set -o pipefail
set -o verbose

time find /mnt2/NAS/ -printf "%k\t%i\t%A+\t%Y\t%p\n" >> index.txt_unsorted
time sort --parallel 8 -n index.txt_unsorted > index.txt
rm -rf index.txt_unsorted

echo index ready
echo DONE

