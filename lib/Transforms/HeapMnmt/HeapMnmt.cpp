//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/TypeFinder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <fstream>
#include <iostream>
using namespace llvm;

#define DEBUG_TYPE "heapMnmt"

namespace {
    struct HeapMnmt : public ModulePass {
        static char ID; // Pass identification, replacement for typeid
        HeapMnmt() : ModulePass(ID) {}

        void printTypes(Module &M) {
            TypeFinder types;
            types.run(M, true);
            for(auto i = types.begin(); i != types.end(); i++) {
                (*i)->dump();
            }
        }

        virtual void getAnalysisUsage(AnalysisUsage &AU) const {
            //AU.addRequired<CallGraphWrapperPass>();
            //AU.addRequired<LoopInfo>();
        }

        bool runOnModule(Module &mod) override {
            Function *main = mod.getFunction("main");
            Function *heapCacheInit = mod.getFunction("_heap_cache_init");
            LLVMContext &context = mod.getContext();
            IRBuilder <> builder(mod.getContext());

            /*
            LoadInst *inst = builder.CreateLoad(hcache);
            */
            //Add _hcache in the global context
            /*
            GlobalVariable *_hcache = mod.getGlobalVariable("_hcache");
            StructType *type = mod.getTypeByName("struct._cache_t");
            if(type == nullptr) {
                errs() << "no type named struct._cache_t\n";
                return false;
            }
            GlobalVariable *hcache = new GlobalVariable(
                    mod, 
                    type, 
                    false, 
                    GlobalValue::ExternalLinkage, 
                    0);
            for(auto i = mod.getGlobalList().begin();
                    i != mod.getGlobalList().end(); i++) {
                (i)->dump();
            }
            */
            /*
            for(auto i = heapCacheInit->arg_begin(); i != heapCacheInit->arg_end(); i++) {
                i->dump();
            }
            */
            std::ifstream ifs;
            ifs.open("_config", std::fstream::in);
            assert(ifs.good());

            int _capacity, _assoc, _blk_size;
            ifs >> _capacity;
            ifs >> _assoc;
            ifs >> _blk_size;


            //Call hcache init function right after the main is called
            std::vector<Value *> call_args;
            builder.SetInsertPoint(main->getEntryBlock().getFirstNonPHI());
            Value *capacity = builder.getInt64(_capacity);
            Value *assoc = builder.getInt64(_assoc);
            Value *blk_size = builder.getInt64(_blk_size);
            call_args.push_back(capacity);
            call_args.push_back(assoc);
            call_args.push_back(blk_size);
            builder.CreateCall(heapCacheInit, call_args);

            //Find malloc and wrap it
            /*
            Function *wrapper = mod.getFunction("wrap");
            for(auto mi = mod.begin(); mi != mod.end(); mi++) {
                Function *f = mi;
                if(f->getName() == "wrap") continue;
                for(auto fi = f->begin(); fi != f->end(); fi++) {
                    BasicBlock *b = fi;
                    for(auto bi = b->begin(); bi != b->end(); bi++) {
                        CallInst *inst;
                        if(!(inst = dyn_cast<CallInst>(bi))) continue;
                        Function *callee = inst->getCalledFunction();
                        if(!callee) continue;
                        if(callee->getName() == "malloc") {
                            std::vector<Value *> call_args;
                            for (unsigned int i = 0, num = inst->getNumArgOperands(); i < num; i++) {
                                Value *arg = inst->getArgOperand(i);
                                call_args.push_back(arg);
                            }
                            Instruction *wrapped = CallInst::Create(wrapper, call_args);
                            ReplaceInstWithInst(inst->getParent()->getInstList(), bi, wrapped);
                            //ReplaceInstWithInst(inst, CallInst::Create(wrapper, inst->getArgOperand(0)));
                            //ReplaceInstWithValue(inst->getParent()->getInstList(), bi, wrapped);
                        }
                    }
                }
            }
            */

            //Find all loads and stores and build address translation instructions if necessary
            Function *g2l = mod.getFunction("_g2l");
            for(auto mi = mod.begin(); mi != mod.end(); mi++) {
                Function *f = mi;
                if(f->getName() == "wrap") continue;
                if(f->getName() != "main") continue;
                for(auto fi = f->begin(); fi != f->end(); fi++) {
                    BasicBlock *b = fi;
                    for(auto bi = b->begin(); bi != b->end(); bi++) {
                        std::vector<Value *> call_args;
                        Value *orig_addr, *opcode, *store_data;
                        Instruction *inst, *new_inst;
                        inst = bi;
                        if(LoadInst *load_inst = dyn_cast<LoadInst>(inst)) {
                            //inst is load inst
                            orig_addr = load_inst->getPointerOperand();
                            opcode =builder.getInt32(0);
                        } else if(StoreInst *store_inst = dyn_cast<StoreInst>(inst)) {
                            //inst is store inst
                            orig_addr = store_inst->getPointerOperand();
                            store_data = store_inst->getValueOperand();
                            opcode = builder.getInt32(1);
                        } else {
                            //inst is neither load nor store; skip this instruction
                            continue;
                        }
                        builder.SetInsertPoint(inst);

                        //If target address is allocated by alloca, pass address translation
                        if(!needAddressTranslation(orig_addr)) {
                            continue;
                        }

                        //Build address translation instructions
                        //call g2l
                        Value *orig_addr_i8 = builder.CreateBitCast(orig_addr,
                                PointerType::get(IntegerType::get(context, 8), 0));
                        call_args.push_back(orig_addr_i8);
                        call_args.push_back(opcode);
                        Value *new_addr = builder.CreateCall(g2l, call_args);

                        //Cast return value of g2l into original type
                        Value *new_addr_original = builder.CreateBitCast(new_addr, orig_addr->getType());

                        //Replace original load/store with new address
                        if(isa<LoadInst>(inst)) {
                            new_inst = new LoadInst(new_addr_original);
                        } else {
                            new_inst = new StoreInst(store_data, new_addr_original);
                        }
                        ReplaceInstWithInst(inst->getParent()->getInstList(), bi, new_inst);
                    }
                }
            }

            /*
            CallGraph &cg = getAnalysis<CallGraphWrapperPass>().getCallGraph();
            for(auto cgi = cg.begin(); cgi != cg.end(); cgi++) {
                CallGraphNode *cgn;
                if(!(cgn = dyn_cast<CallGraphNode>(cgi->second))) {
                    continue;
                }
                Function *caller = cgn->getFunction();
                //skip external nodes
                if(!caller) continue;

                for (CallGraphNode::iterator cgni = cgn->begin(), cgne = cgn->end(); cgni != cgne; cgni++) {
                    CallInst *call_inst = dyn_cast <CallInst> (cgni->first);
                    BasicBlock::iterator ii(call_inst);
                    CallGraphNode *called_cgn = dyn_cast <CallGraphNode> (cgni->second);
                    Function *callee = called_cgn->getFunction();
                    assert(call_inst && called_cgn);

                    builder.SetInsertPoint(call_inst);
                    errs() << callee->getName() << "\n";
                }
            }
            */
            return false;
        }

        bool needAddressTranslation(Value *addr) {
            if(PHINode *inst = dyn_cast<PHINode>(addr)) {
                int nIncomingValues = inst->getNumIncomingValues();
                for(int i=0; i<nIncomingValues; i++) {
                    Value *new_addr = inst->getIncomingValue(i);
                    if(needAddressTranslation(new_addr)) {
                        return true;
                    }
                }
                return false;
            }
            if(GetElementPtrInst *inst = dyn_cast<GetElementPtrInst>(addr)) {
                Value *new_addr = inst->getPointerOperand();
                return needAddressTranslation(new_addr);
            }

            if(isa<AllocaInst>(addr) || isa<Constant>(addr)) {
                return false;
            }
            return true;
        }
    };
}

char HeapMnmt::ID = 0;
static RegisterPass<HeapMnmt> X("smmhm", "Heap Management Pass");
