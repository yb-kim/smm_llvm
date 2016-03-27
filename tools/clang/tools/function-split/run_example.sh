#!/bin/bash
LLVM_DIR=~/workspace/code_management/smm_llvm_build  #the location of your llvm dir
CLANG_DIR=$LLVM_DIR/tools/clang/tools/function-split

rm -f test_modified.c test_orig test_modified output_orig.txt output_modified.txt

cp ./* $CLANG_DIR
cd $CLANG_DIR
make && \
#$LLVM_DIR/Debug+Asserts/bin/functionSplitter test_orig.c -- 2> test_modified.c && \
$LLVM_DIR/Debug+Asserts/bin/functionSplitter test_orig.c -- 
exit
gcc -o test_orig test_orig.c && \
./test_orig > output_orig.txt && \
gcc -o test_modified test_modified.c && \
./test_modified > output_modified.txt && \
echo "output_orig: " && \
cat output_orig.txt && \
echo "output_modified: " && \
cat output_modified.txt && \
diff output_orig.txt output_modified.txt
