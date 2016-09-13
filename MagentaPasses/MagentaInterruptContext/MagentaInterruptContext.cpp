//===- InterruptContextPass.cpp -------------------------------------------===//
//
// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
//===----------------------------------------------------------------------===//
//
// This file implements the a pass that explores all the paths from a set of
// source functions to a set of sink functions, and prints all the function
// calls within those paths. The purpose of this pass is to print all the
// functions called within the interrupt context in magenta code base. The pass
// produces a warning when reaching calls that are not supposed to be made
// within the interrupt context, and prints the path reaching the call.
//
////===----------------------------------------------------------------------===//

#include "cxxabi.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <set>

using namespace llvm;

namespace {
class InterruptContext : public ModulePass {
public:
  static char ID;
  InterruptContext() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;

private:
  /// Traverses all the basic blocks within a function using
  /// depth-first-search. Returns false if it reaches a black-listed function
  /// along the path. Otherwise, returns true.
  bool traverseBasicBlock(BasicBlock &Block);
  /// For each basic block, goes through all the instructions within the block
  /// and finds calls to functions. For each of such calls, starts a
  /// depth-first-search from that function. Returns false if it reaches a
  /// black-listed function along the path. Otherwise, it returns true.
  bool examineBlock(BasicBlock &Block);
  /// Gets the first basic block of the function and starts depth-first-search
  /// traversal from it. Returns false if it reaches a black-listed function
  /// along the path. Otherwise, it returns true.
  bool traverseFunction(Function *F);
  /// Produces a warning message, containing the call chain that reaches a
  /// black-listed function.
  void warnOnCallChain();

  SmallPtrSet<const BasicBlock *, 32> BasicBlockPtrSet;
  SmallPtrSet<const Function *, 32> FuncPtrSet;
  std::vector<std::string> FunctionChain;
  const std::set<std::string> BlackListSet = {"mutex_acquire",
                                              "mutex_acquire_timeout",
                                              "mutex_acquire_timeout_internal"};
  /// We terminate a path if we reach anything from SinkFunctionSet
  const std::set<std::string> SinkFunctionSet = {"thread_preempt", "panic",
                                                 "_panic"};
  /// This represents the beginning of interrupt context
  const std::string SourceFunction = "x86_exception_handler";
};
}

char InterruptContext::ID = 0;

static RegisterPass<InterruptContext> X("interruptContext",
                                        "List of function calls", false, false);

bool InterruptContext::runOnModule(Module &M) {
  Function *Source = M.getFunction(SourceFunction);
  if (Source)
    traverseFunction(Source);
  return false;
}

bool InterruptContext::traverseBasicBlock(BasicBlock &Block) {
  bool Passed = true;
  if (BasicBlockPtrSet.count(&Block) != 0)
    return Passed;

  BasicBlockPtrSet.insert(&Block);
  Passed &= examineBlock(Block);
  const TerminatorInst *TermInst = Block.getTerminator();
  if (!TermInst)
    return Passed;

  for (unsigned i = 0; i < TermInst->getNumSuccessors(); ++i) {
    BasicBlock *SuccBlock = TermInst->getSuccessor(i);
    if (SuccBlock)
      Passed &= traverseBasicBlock(*SuccBlock);
  }

  return Passed;
}

bool InterruptContext::examineBlock(BasicBlock &Block) {
  bool Passed = true;
  for (BasicBlock::iterator Inst = Block.begin(); Inst != Block.end(); ++Inst) {
    CallInst *CInst = dyn_cast<CallInst>(cast<Instruction>(Inst));
    if (!CInst)
      continue;

    Function *F = CInst->getCalledFunction();
    if (!F)
      continue;

    Passed &= traverseFunction(F);
  }

  return Passed;
}

bool InterruptContext::traverseFunction(Function *F) {
  bool Passed = true;

  if (FuncPtrSet.count(F) != 0)
    return Passed;

  std::string Name;

  // In case the function is a C++ function, the name will be mangled. So we
  // need to dismangle the name. In this case, unlike the C function, the name
  // will contain the class it belongs to (if exists any), along with the
  // argument types. The format of the name is:
  // className::functionName(argTyp1, ..., argTypeN)
  // As C function names are not mangled (no overloading is allowed in C),
  // C function names have no argument info.
  char *DemangledName =
      __cxxabiv1::__cxa_demangle(F->getName().str().c_str(), NULL, NULL, NULL);
  if (DemangledName)
    Name = DemangledName;
  else
    Name = F->getName().str();

  if (SinkFunctionSet.count(Name))
    return Passed;

  FunctionChain.push_back(Name);
  if (BlackListSet.count(Name)) {
    Passed = false;
    warnOnCallChain();
    FunctionChain.pop_back();
    return Passed;
  }

  FuncPtrSet.insert(F);
  if (F->size()) {
    BasicBlock &FirstBlock = F->getEntryBlock();
    Passed &= traverseBasicBlock(FirstBlock);
  }

  FunctionChain.pop_back();
  return Passed;
}

void InterruptContext::warnOnCallChain() {
  errs() << "Reached a black-listed function via the following call chain:";
  for (auto F : FunctionChain)
    errs() << " " << F;
  errs() << "\n";
}
