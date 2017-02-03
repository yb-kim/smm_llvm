LLVM_DIR=/home/ybkim/workspace/spm/smm_llvm

cp ./* $LLVM_DIR/build/lib/Transforms/HeapMnmt/
cd $LLVM_DIR/build/lib/Transforms/HeapMnmt
make
