#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalIFunc.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/Transforms/Utils/HookFunctionPointers.h"

using namespace llvm;

// Enable our pass
static cl::opt<bool>
WrapFunctionPointerCreation("wrap-function-pointer-creation",
                            cl::desc("Wrap function pointer creation with a call to external function __ocolos_function_pointer_hook"),
                            cl::init(false), cl::Hidden);


PreservedAnalyses HookFunctionPointersPass::run(Function &F, FunctionAnalysisManager &AM) {
  if (!WrapFunctionPointerCreation) {
    return PreservedAnalyses::all();
  }

  TargetLibraryInfo tli = AM.getResult<TargetLibraryAnalysis>(F);
  
  //errs() << "HookFunctionPointers running on: ";
  //errs().write_escaped(F.getName()) << '\n';

  LibFunc lf;
  SmallPtrSet<Instruction*,4> funcPtrUses;
    
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    //errs() << "[jld] " << *I << "\n";

    if (StoreInst *SI = dyn_cast<StoreInst>(&*I)) {
      Value *val = SI->getValueOperand();

      if (Function *valFun = dyn_cast<Function>(val)) {
      //Value *ptr = SI->getPointerOperand();
        //if (isa<Function>(val)) {
        //Function* valFun = cast<Function>(val);
        //if (Function* valFun = dyn_cast<Function>(val)) {
        //if (PointerType *ptrType = dyn_cast<PointerType>(val->getType())) {
        //  Type *pointeeType = ptrType->getElementType()
        //}

        if (//val->getName().startswith("__cxx_") ||
            //val->getName().startswith("__clang_") ||
            valFun->isIntrinsic() ||
            tli.getLibFunc(*valFun, lf)) {
          errs() << "[HookFunctionPointers] " << *I << " uses a function pointer to an external/builtin function, skipping...\n";
          continue;
        }

        // we have a legit function pointer
        errs() << "[HookFunctionPointers] " << *I << " uses a function pointer\n";
        funcPtrUses.insert(&*I);
      }
    }
    
    // for (Use &U : I->operands()) {
    //   Value *v = U.get();
    //   if (!v->getType()->isPointerTy()) {
    //     continue;
    //   }
    //   PointerType *pointerType = dyn_cast<PointerType>(v->getType());
    //   Type *pointeeType = pointerType->getElementType();
    //   if (pointeeType->isPointerTy() || isa<Function>(pointeeType)) {
    //     continue;
    //   }
      
    //   errs() << "[jld]    operand type is ";
    //   v->getType()->print(errs());
    //   errs() << "\n";
      
    //   if (isa<Function>(v)) {
    //     if (v->getName().startswith("__cxx_") ||
    //         v->getName().startswith("__clang_") ||
    //         cast<Function>(v)->isIntrinsic() ||
    //         tli.getLibFunc(F, lf)) {
    //       errs() << "[HookFunctionPointers] " << *I << " uses a function pointer to an external function, skipping...\n";
    //       continue;
    //     }
    //     errs() << "[HookFunctionPointers] " << *I << " uses a function pointer\n";
    //     funcPtrUses.insert(&*I);
    //   }
    //   if (isa<GlobalIFunc>(v)) {
    //     errs() << "[HookFunctionPointers] " << *I << " uses a global ifunc...?\n";
    //     funcPtrUses.insert(&*I);
    //   }
    // }
    
  } // end first pass over insns

  FunctionCallee hook = F.getParent()->getOrInsertFunction("__ocolos_function_pointer_hook"
                                                           ,Type::getInt64PtrTy(F.getContext())
                                                           ,Type::getInt64PtrTy(F.getContext())
                                                           );
  for (Instruction* fptrI : funcPtrUses) {
    IRBuilder<> builder(fptrI);
    SmallVector<Value*,1> args;
    Value* fptrV = nullptr;
    for (Use &U : fptrI->operands()) {
      Value *v = U.get();
      if (isa<Function>(v)) {
        fptrV = v;
      }
    }
    assert(fptrV && "couldn't find the function pointer");
    
    errs() << "[HookFunctionPointers] starting to instrument " << *fptrI << " ...\n";
    Value* intPtrV = builder.CreatePointerCast(fptrV, Type::getInt64PtrTy(F.getContext()));
    args.push_back(intPtrV);
    CallInst* callI = builder.CreateCall(hook.getFunctionType(), hook.getCallee(), args);
    Value* retFptrV = builder.CreatePointerCast(callI, fptrV->getType());
    
    // cast return value back to fptrV, replace uses in fptrI with that 
    fptrI->replaceUsesOfWith(fptrV, retFptrV);
  }
  
  // we changed the code
  return PreservedAnalyses::none();
}
