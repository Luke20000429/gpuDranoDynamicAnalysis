#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include <llvm/Support/raw_ostream.h>
#include "llvm/IR/LegacyPassManager.h"
// Basename function.
#include <libgen.h>

#include <vector>


using namespace llvm;
using namespace std;

namespace{
  struct DynamicAnalysis : public FunctionPass{
    static char ID;
    DynamicAnalysis() : FunctionPass(ID) {}
    StringRef mangledName;
    int staticId = 0;

    /* Iterate over function in file, inserting instrumentation. */
    virtual bool runOnFunction(Function &fun) override{
      errs() << "Run on function: " << fun.getName() << "\n";
      LLVMContext& ctx = fun.getContext();
      Module* module = fun.getParent();
      StringRef moduleName = module->getName();

      //Find out name of our mangled function.
      StringRef countCachesFun("countCacheLines");
      // TODO: This may fail if there is not debug information available?
      DISubprogram* funDebug = fun.getSubprogram();
      if(!funDebug){
        errs() << "No debug information for function " << fun.getName()
               << " in "  << moduleName << ". Skipping." << '\n';
        return false;
      }

      StringRef fileName = funDebug->getFilename();
      StringRef functionName = funDebug->getName();

      if(countCachesFun == functionName)
        mangledName = StringRef(fun.getName());
      /* Avoid inserting code recursively in our own function by skipping the file.
	 Also, skip intrinsictly defined functions. */
      if(fileName.find("dynamicAnalysis") != StringRef::npos || functionName.startswith("__")){
	      errs() << "Skipping instrumentation into " << functionName << '\n';
        return false;
      }else{
	      errs() << "Instrumenting: " << functionName << '\n';
      }
      
      /* For every function, for every basic block (bb), for every instruction... */
      for(inst_iterator inst = inst_begin(fun), instEnd = inst_end(fun);
          inst != instEnd;
          inst++){
        if(StoreInst* si = dyn_cast<StoreInst>(&*inst)){
	  errs() << "Store Address space: " << si->getPointerAddressSpace() << '\n';
          storeInstruction(si, ctx, module, functionName);
	}
        else if(LoadInst* ls = dyn_cast<LoadInst>(&*inst)){
          loadInstruction(ls, ctx, module, functionName);
	  errs() << "Load Address space: " << ls->getPointerAddressSpace() << '\n';
	}
      }

      return true;
    }

    /* Insert instrumentation for store instruction. */
    /* This is an exact mirror copy of loadInstruction. */
    void storeInstruction(StoreInst* si, LLVMContext& ctx, Module* module,
                          StringRef functionName){
      IRBuilder<> builder(si);
      DebugLoc loc = si->getDebugLoc();
      StringRef moduleName = module->getName();
      /* No debug information found, skip! */
      if(! loc)
        return;

      StringRef filename = loc->getFilename();
      int lineNum = loc->getLine();
      int columnNum = loc->getColumn();

      // We dont want to insert calls to the instrumentation in the instrumentation
      // code itself. Skip!
      if(filename.endswith("./dynamicAnalysisCode.cu"))
        return;
      // This is needed, otherwise we end with unnamed instructions being included into the code :/
      if(mangledName == "")       // Safety check...
        return;

      // Casting to i8 pointer.
      Value* val = si->getPointerOperand();
      Value* castedVal = builder.CreateBitCast(val, Type::getInt8PtrTy(ctx));

      // Insert here!
      FunctionCallee  myFun = insertInstrumentingFunc(mangledName, ctx, module);

      ConstantInt *line = builder.getInt32(lineNum);
      ConstantInt *column = builder.getInt32(columnNum);

      // Assign unique static identifier per function call.
      ConstantInt *myStaticId = builder.getInt32(staticId);
      staticId++;

      // Find the size of the data we are accessing.
      Type* baseType = si->getValueOperand()->getType();
      DataLayout* dl = new DataLayout(module);
      int typeSize = (int) dl->getTypeStoreSize(baseType); 
      ConstantInt *typeSizeB = builder.getInt32(typeSize);
      errs() << *baseType << ' ' << typeSize << '\n';

      Value* moduleVal = builder.CreateGlobalStringPtr(moduleName);
      Value* functionVal = builder.CreateGlobalStringPtr(functionName);
      Value* storeVal = builder.CreateGlobalStringPtr("store");
      Value* args[] = {castedVal, moduleVal, functionVal, storeVal, line, column,
                       myStaticId, typeSizeB};
      builder.CreateCall(myFun, args);
      errs() << "Generated: "; (builder.CreateCall(myFun, args))->print(errs());
      errs() << '\n';

      return;
    }

    /* Insert instrumentation for store instruction. */
    /* This is an exact mirror copy of storeInstruction. */
    void loadInstruction(LoadInst* si, LLVMContext& ctx, Module* module,
                          StringRef functionName){
      IRBuilder<> builder(si);
      DebugLoc loc = si->getDebugLoc();
      StringRef moduleName = module->getName();
      /* No debug information found, skip! */
      if(! loc)
        return;

      StringRef filename = loc->getFilename();
      int lineNum = loc->getLine();
      int columnNum = loc->getColumn();

      // We dont want to insert calls to the instrumentation in the instrumentation
      // code itself. Skip!
      if(filename.endswith("./dynamicAnalysisCode.cu"))
        return;
      if(mangledName == "")       // Safety check...
        return;

      // Casting to i8 pointer.
      Value* val = si->getPointerOperand();
      Value* castedVal = builder.CreateBitCast(val, Type::getInt8PtrTy(ctx));

      FunctionCallee myFun = insertInstrumentingFunc(mangledName, ctx, module);
      ConstantInt *line = builder.getInt32(lineNum);
      ConstantInt *column = builder.getInt32(columnNum);

      // Assign unique static identifier per function call.
      ConstantInt *myStaticId = builder.getInt32(staticId);
      staticId++;

      // Find the size of the data we are accessing.
      Type* baseType = si->getType();
      DataLayout* dl = new DataLayout(module);
      int typeSize = (int) dl->getTypeStoreSize(baseType); 
      ConstantInt *typeSizeB = builder.getInt32(typeSize);
      errs() << *baseType << ' ' << typeSize << '\n';


      Value* moduleVal = builder.CreateGlobalStringPtr(moduleName);
      Value* functionVal = builder.CreateGlobalStringPtr(functionName);
      Value* storeVal = builder.CreateGlobalStringPtr("load");
      Value* args[] = {castedVal, moduleVal, functionVal, storeVal, line, column,
                       myStaticId, typeSizeB};
      builder.CreateCall(myFun, args);
      errs() << "Generated: "; (builder.CreateCall(myFun, args))->print(errs());
      errs() << '\n';

      return;
    }

    /**
     * Does actual function insertion at *this* location.
     */
    FunctionCallee insertInstrumentingFunc(StringRef mangledName, LLVMContext& ctx,
                                      Module* module){
      Type* returnType = Type::getVoidTy(ctx);
      vector<Type*> fArgs { Type::getInt8PtrTy(ctx), Type::getInt8PtrTy(ctx),
          Type::getInt8PtrTy(ctx), Type::getInt8PtrTy(ctx),
          Type::getInt32Ty(ctx), Type::getInt32Ty(ctx),
          Type::getInt32Ty(ctx), Type::getInt32Ty(ctx)
          };
      FunctionType* instrumenter = FunctionType::get(returnType, fArgs, false);
      return module->getOrInsertFunction(mangledName, instrumenter);
    }

    ~DynamicAnalysis(){
      errs() << "End of dynamic analysis instrumentation pass.\n";
    }

  };
}

char DynamicAnalysis::ID = 0;
static RegisterPass<DynamicAnalysis> X("dynamicAnalysis", "Dynamic Analysis.", false, false);

static void registerMyPass(const PassManagerBuilder &,
                           legacy::PassManagerBase &PM){
  PM.add(new DynamicAnalysis());
}
static RegisterStandardPasses
RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
               registerMyPass);
