#include <map>
#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "util.h"
#include "struct_filter.h"

using namespace llvm;

static const std::string PREFIX = "mypass";

std::string funcStubName(const std::string &struct_name, size_t idx) {
    return PREFIX + "_" + struct_name + "_" + std::to_string(idx) + "_stub";
}

std::string structSingletonName(const std::string &struct_name) {
    return PREFIX + "_" + struct_name + "_singleton";
}

struct StructVisitorPass : public ModulePass {
    StructVisitorPass(): ModulePass(ID) {}

    bool runOnModule(Module &M) override {
        type_tracker = struct_filter(&M);
        outs().flush();

        createGlobalInitializer(M);
        createFunctionCaller(M);

        // Create all possible stubs to be ready for function calls
        for (auto type : M.getIdentifiedStructTypes()) {
            createSingleton(M, type);
        }
        outs() << "Singletons created\n";
        outs().flush();

        fillSingletons(M);
        outs() << "Singletons filled\n";
        outs().flush();

        detectAllGlobals(M);
        outs() << "Globals resolved\n";
        outs().flush();

        propagateSingletons(M);
        outs() << "Singletons pushed\n";
        outs().flush();

        implementAllInterestingDeclarations(M);
        outs() << "Functions implemented\n";
        outs().flush();

        finalizeGlobalInitializer(M);
        finalizeFunctionCaller(M);

        // Clean type_tracker
        type_tracker = struct_filter();
        return true;
    }

    static char ID;
private:
    // Interesting types
    struct_filter type_tracker;
    // data consumer objects for interesting types
    std::unordered_map<StructType*, GlobalVariable*> singletons;
    // field stubs
    std::map<std::pair<StructType*, size_t>, Function*> function_stubs;
    // all added functions
    std::set<Function*> new_functions;

    // Singletons initializer
    Function* global_initializer = nullptr;
    // Initializer function block
    BasicBlock* global_initializer_bb = nullptr;

    // Function caller
    Function* functions_caller = nullptr;
    // Function caller block
    BasicBlock* functions_caller_bb = nullptr;

    // Check if any function argument or return value are interesting
    bool functionContainsInterestingStruct(FunctionType *f_type) {
        if (type_tracker.isInterestingTypeOrPtr(f_type->getReturnType())) {
            return true;
        }
        return std::ranges::any_of(
                f_type->params(),
                [this](Type *t) { return type_tracker.isInterestingTypeOrPtr(t); }
        );
    }

    // Create an argument default value
    // Flow-sensitivity is not expected from the following analysis, so it's fine to put any fitting value
    // But here we try to use singletons as much as we can
    Value* constructTypeValue(Type *t, IRBuilder<> &builder) {
        if (t->isIntegerTy()) {
            return builder.getIntN(t->getIntegerBitWidth(), 0);
        }
        if (t->isStructTy()) {
            // We added all singletons, there should be one for this struct
            auto obj = singletons[dyn_cast<StructType>(t)];
            if (!obj) {
                // Special case for timespec64 return type
                return Constant::getNullValue(dyn_cast<StructType>(t));
            }
            return obj;
        }
        if (t->isPointerTy()) {
            auto underlying_t = t->getNonOpaquePointerElementType();
            if (type_tracker.isInterestingType(underlying_t)) {
                return singletons[dyn_cast<StructType>(underlying_t)];
            } else {
                return ConstantPointerNull::get(dyn_cast<PointerType>(t));
            }
        }
        outs() << "Unknown type!\n";
        outs().flush();
        t->dump();
        outs().flush();
        exit(1);
    }

    // Make a call to the function and save the return value, if needed
    // Whenever it is possible, singletons are used as arguments
    void createDummyFunctionCall(Module &M, Function *f) {
        LLVMContext &ctx = M.getContext();
        IRBuilder<> builder(ctx);
        builder.SetInsertPoint(functions_caller_bb);

        std::vector<Value*> call_args;
        for (auto arg : f->getFunctionType()->params()) {
            call_args.push_back(constructTypeValue(arg, builder));
        }
        auto call_ret = builder.CreateCall(f, call_args);

        // Big structures are not usually returned by value
        // Instead, function accepts a pointer to return value
        if (type_tracker.isInterestingType(f->getReturnType())) {
            outs() << "WARNING: Function returning interesting type by value!\n";
            outs().flush();
            f->getFunctionType()->dump();
            outs().flush();
            return;
        }
        if (type_tracker.isPtrToInterestingType(f->getReturnType())) {
            auto* ret_struct = dyn_cast<StructType>(f->getReturnType()->getNonOpaquePointerElementType());
            builder.CreateMemCpy(
                singletons[ret_struct],
                MaybeAlign(),
                call_ret,
                MaybeAlign(),
                M.getDataLayout().getTypeAllocSize(ret_struct)
            );
        }
    }

    // Make calls to all functions, that consume or produce interesting structures
    // to track written and read values. Currently internal functions are skipped as they
    // cannot be called from outside.
    void propagateSingletons(Module &M) {
        for (auto &f : M.getFunctionList()) {
            if (new_functions.contains(&f)) {
                // Do not instrument newly created functions
                continue;
            }
            // Skip functions without body
            if (f.isDeclaration()) {
                continue;
            }
            // Skip hidden functions, they are not visible from outside
            if (f.hasInternalLinkage() || f.hasPrivateLinkage()) {
                continue;
            }
            if (!functionContainsInterestingStruct(f.getFunctionType())) {
                continue;
            }
            outs().flush();
            createDummyFunctionCall(M, &f);
        }
    }

    void implementAllInterestingDeclarations(Module &M) {
        for (auto &f : M.getFunctionList()) {
            if (functionContainsInterestingStruct(f.getFunctionType()) && f.isDeclaration()) {
                createStubForDeclaredFunction(M, &f);
            }
        }
    }

    // Temporal fix for tracking global values directly
    // See: https://github.com/SVF-tools/SVF/issues/1650
    void detectAllGlobals(Module &M) {
        IRBuilder<> builder(M.getContext());
        builder.SetInsertPoint(global_initializer_bb);

        for (auto &glob : M.getGlobalList()) {
            if (!type_tracker.isInterestingType(glob.getValueType())) {
                continue;
            }
            auto *type = dyn_cast<StructType>(glob.getValueType());
            auto singleton = singletons[type];
            if (&glob == singleton) {
                continue;
            }
            outs() << "Detected interesting global " << glob.getName() << "\n";
            // obj -> singleton
            copyStructBetweenPointers(M, builder, type, &glob, singleton);
            // maybe singleton -> obj?
        }
        outs().flush();
    }

    // Iterate over all structures and instrument all interesting fields
    // In case of function pointer - create a stub for it and store it into the singleton
    void fillSingletons(Module &M) {
        for (auto interesting_t : type_tracker.getInterestingTypes()) {
            // Dummy object is already created for this type
            for (size_t i = 0; i < interesting_t->getNumElements(); i++) {
                auto field = interesting_t->getElementType(i);
                if (isFunctionPointer(field)) {
                    createStubFunction(M, interesting_t, i);
                }
            }
            initializeStructureFields(M, interesting_t);
        }
    }

    // This field is interesting, initialize it
    // In case of function pointer store the corresponding stub for it
    void initializeStructureFields(Module &M, StructType *T) {
        // Used for nested interesting structs
        IRBuilder<> builder(M.getContext());
        builder.SetInsertPoint(global_initializer_bb);

        std::vector<Constant*> new_init;

        for (size_t i = 0; i < T->getNumElements(); i++) {
            auto field_type = T->getElementType(i);
            if (isFunctionPointer(field_type)) {
                new_init.push_back(function_stubs[{T, i}]);
            } else if (type_tracker.isInterestingType(field_type)) {
                // Zero-initialize field in struct definition
                new_init.push_back(Constant::getNullValue(field_type));

                // Share data between singleton and this field
                auto subtype_singleton = singletons[dyn_cast<StructType>(field_type)];
                Value *ptr_gep = builder.CreateStructGEP(T, singletons[T], i);
                // Store written values into the underling singleton
                copyStructBetweenPointers(M, builder, field_type, ptr_gep, subtype_singleton);
                // Extract written values from underlying singleton to the outer structure
                copyStructBetweenPointers(M, builder, field_type, subtype_singleton, ptr_gep);
            } else if (type_tracker.isPtrToInterestingType(field_type)) {
                auto subtype_singleton = singletons[
                        dyn_cast<StructType>(field_type->getNonOpaquePointerElementType())
                ];
                new_init.push_back(subtype_singleton);
            } else {
                new_init.push_back(Constant::getNullValue(field_type));
            }
        }

        singletons[T]->setInitializer(
            ConstantStruct::get(T, new_init)
        );
    }

    // Create a singleton with a given type and no fields
    // Created objects are marked external to avoid initialization
    GlobalVariable* createSingleton(Module &M, StructType *T) {
        if (singletons.contains(T)) {
            return singletons[T];
        }
        // Mark is as external to avoid initializing structure
        auto *var = new GlobalVariable(
                M, T, false, GlobalValue::InternalLinkage,
                ConstantStruct::getNullValue(T), // Definition is required for internal linkage
                structSingletonName(T->getName().operator std::string())
        );
        singletons[T] = var;
        return var;
    }

    // Implement all declared functions to collect passed values
    void createStubForDeclaredFunction(Module &M, Function *f) {
        auto f_type = f->getFunctionType();
        IRBuilder<> builder(M.getContext());
        BasicBlock *bb = BasicBlock::Create(M.getContext(), "", f);
        builder.SetInsertPoint(bb);

        size_t i = 0;
        for (auto arg : f_type->params()) {
            if (type_tracker.isPtrToInterestingType(arg)) {
                copyStructBetweenPointers(
                    M, builder, dereferenceStructPtr(arg),
                    f->getArg(i), singletons[dereferenceStructPtr(arg)]
                );
                copyStructBetweenPointers(
                        M, builder, dereferenceStructPtr(arg),
                        singletons[dereferenceStructPtr(arg)], f->getArg(i)
                );
            } else if (type_tracker.isInterestingType(arg)) {
                outs() << "WARNING: Function getting interesting type by value!!\n";
                outs().flush();
                // TODO: add value-pass support
            }
            i++;
        }
        if (f_type->getReturnType()->isVoidTy()) {
            builder.CreateRetVoid();
        } else {
            auto ret_value = constructTypeValue(f_type->getReturnType(), builder);
            builder.CreateRet(ret_value);
        }
    }

    // Create a stub function, that will track field_idx field in a StructType T
    // This function reads corresponding field from the singleton and calls it
    // Its type is the same as in the structure, so arguments are just forwarded and the same return value is used
    Function* createStubFunction(Module &M, StructType *T, size_t field_idx) {
        auto name = funcStubName(T->getName().operator std::string(), field_idx);
        FunctionType *stub_type = dereferenceFPtr(T->getTypeAtIndex(field_idx));
        if (!stub_type) {
            outs() << "createStubFunction: struct " << T->getName() << " does not have function pointer at field ";
            outs() << field_idx << "\n";
            outs().flush();
            exit(1);
        }
        Function *f = Function::Create(
            stub_type, Function::ExternalLinkage,
            name, M
        );
        LLVMContext &ctx = M.getContext();
        IRBuilder<> builder(ctx);
        BasicBlock *body = BasicBlock::Create(ctx, "", f);

        std::vector<Value*> args;
        for (size_t i = 0; i < f->arg_size(); i++) {
            args.push_back(f->getArg(i));
        }

        builder.SetInsertPoint(body);

        Value *ptr_gep = builder.CreateStructGEP(T, singletons[T], field_idx);
        Value *fptr = builder.CreateLoad(stub_type->getPointerTo(), ptr_gep);
        // fptr contains field value from singleton
        Value* call = builder.CreateCall(stub_type, fptr, args);

        if (stub_type->getReturnType()->isVoidTy()) {
            builder.CreateRetVoid();
        } else {
            builder.CreateRet(call);
        }

        function_stubs[{T, field_idx}] = f;
        new_functions.insert(f);
        return f;
    }

    // Function initializers and finalizers

    void createGlobalInitializer(Module &M) {
        std::string initializer_name = PREFIX + "_global_initializer";
        LLVMContext &ctx = M.getContext();
        global_initializer = Function::Create(
                FunctionType::get(Type::getVoidTy(ctx), false),
                Function::ExternalLinkage,
                initializer_name,
                M
        );
        global_initializer_bb = BasicBlock::Create(ctx, "", global_initializer);
        new_functions.insert(global_initializer);
    }

    void finalizeGlobalInitializer(Module &M) {
        LLVMContext &ctx = M.getContext();
        IRBuilder<> builder(ctx);
        builder.SetInsertPoint(global_initializer_bb);
        builder.CreateRetVoid();
    }

    void createFunctionCaller(Module &M) {
        std::string initializer_name = PREFIX + "_function_caller";
        LLVMContext &ctx = M.getContext();
        functions_caller = Function::Create(
                FunctionType::get(Type::getVoidTy(ctx), false),
                Function::ExternalLinkage,
                initializer_name,
                M
        );
        functions_caller_bb = BasicBlock::Create(ctx, "", functions_caller);
        new_functions.insert(functions_caller);
    }

    void finalizeFunctionCaller(Module &M) {
        LLVMContext &ctx = M.getContext();
        IRBuilder<> builder(ctx);
        builder.SetInsertPoint(functions_caller_bb);
        builder.CreateRetVoid();
    }
};

char StructVisitorPass::ID = 0;

static RegisterPass<StructVisitorPass> X("instr", "Instrument Structs Pass",
                                         false /* Only looks at CFG */,
                                         false /* Analysis Pass */);

static RegisterStandardPasses Y(
    PassManagerBuilder::EP_OptimizerLast,
    [](const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
        PM.add(new StructVisitorPass());
    }
);
