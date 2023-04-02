#ifndef LLVM_TRANSFORMS_HOOK_FUNCTION_POINTERS_H
#define LLVM_TRANSFORMS_HOOK_FUNCTION_POINTERS_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class HookFunctionPointersPass : public PassInfoMixin<HookFunctionPointersPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_HOOK_FUNCTION_POINTERS_H
