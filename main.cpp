#include <map>
#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

static const std::string PREFIX = "mypass";

bool isFunctionPointer(Type *t) {
    if (auto *PT = dyn_cast<PointerType>(t)) {
        // Check if the element type of the pointer is a function type
        if (dyn_cast<FunctionType>(PT->getNonOpaquePointerElementType())) {
            return true;
        }
    }
    return false;
}

FunctionType* dereferenceFPtr(Type *t) {
    if (auto *PT = dyn_cast<PointerType>(t)) {
        // Check if the element type of the pointer is a function type
        if (auto type = dyn_cast<FunctionType>(PT->getNonOpaquePointerElementType())) {
            return type;
        }
    }
    return nullptr;
}

std::string funcStubName(const std::string &struct_name, size_t idx) {
    return PREFIX + "_" + struct_name + "_" + std::to_string(idx) + "_stub";
}

std::string structSingletonName(const std::string &struct_name) {
    return PREFIX + "_" + struct_name + "_singleton";
}

struct StructVisitorPass : public ModulePass {
    StructVisitorPass(): ModulePass(ID) {}

    bool runOnModule(Module &M) override {
        inspectStructs(M);
        outs().flush();

        createGlobalInitializer(M);
        createFunctionCaller(M);

        // Create all possible stubs to be ready for function calls
        for (auto type : M.getIdentifiedStructTypes()) {
            createSingleton(M, type);
            outs() << "Singleton for " << type->getName() << " created\n";
        }
        outs() << "Singletons created\n";
        outs().flush();

        fillSingletons(M);
        outs() << "Singletons filled\n";
        outs().flush();

        propagateSingletons(M);

        finalizeGlobalInitializer(M);
        finalizeFunctionCaller(M);
        return true;
    }

    static char ID;
private:
    // Types with reachable function pointers
    std::set<StructType*> interesting_types;
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

    bool isInterestingType(Type *t) {
        if (auto *ret = dyn_cast<StructType>(t)) {
            return interesting_types.contains(ret);
        }
        return false;
    }

    bool isInterestingTypeOrPtr(Type *t) {
        if (isInterestingType(t)) {
            return true;
        }
        return t->isPointerTy() && isInterestingType(t->getNonOpaquePointerElementType());
    }

    bool functionContainsInterestingStruct(FunctionType *f_type) {
        if (isInterestingTypeOrPtr(f_type->getReturnType())) {
            return true;
        }
        return std::ranges::any_of(
                f_type->params(),
                [this](Type *t) { return isInterestingTypeOrPtr(t); }
        );
    }

    Value* constructTypeValue(Type *t, IRBuilder<> &builder) {
        if (t->isIntegerTy()) {
            return builder.getIntN(t->getIntegerBitWidth(), 0);
        }
        if (t->isStructTy()) {
            // We added all singletons, there should be one for this struct
            auto obj = singletons[dyn_cast<StructType>(t)];
            if (!obj) {
                outs() << "EMERGENCY! This struct type has no singleton:\n";
                outs().flush();
                t->dump();
                outs().flush();
                exit(1);
            }
            return obj;
        }
        if (t->isPointerTy()) {
            auto underlying_t = t->getNonOpaquePointerElementType();
            if (isInterestingType(underlying_t)) {
                return builder.CreateLoad(t, singletons[dyn_cast<StructType>(underlying_t)]);
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

    void createDummyFunctionCall(Module &M, Function *f) {
        LLVMContext &ctx = M.getContext();
        IRBuilder<> builder(ctx);
        builder.SetInsertPoint(functions_caller_bb);

        std::vector<Value*> call_args;
        for (auto arg : f->getFunctionType()->params()) {
            call_args.push_back(constructTypeValue(arg, builder));
        }
        auto call_ret = builder.CreateCall(f, call_args);

        if (isInterestingType(f->getReturnType())) {
            // Contract: singletons exist for interesting type
            // TODO: add store code
        }
    }

    void propagateSingletons(Module &M) {
        for (auto &f : M.getFunctionList()) {
            if (new_functions.contains(&f)) {
                // Do not instrument newly created functions
                continue;
            }
            // Skip hidden functions, they are not visible from outside
            if (f.hasInternalLinkage() || f.hasPrivateLinkage()) {
                outs() << "Dropping function " << f.getName() << " due to hidden linkage\n";
                outs().flush();
                continue;
            }
            if (!functionContainsInterestingStruct(f.getFunctionType())) {
                outs() << "Dropping function " << f.getName() << " due to boring args\n";
                outs().flush();
                continue;
            }
            outs() << "Making call for " << f.getName() << "\n";
            outs().flush();
            createDummyFunctionCall(M, &f);
        }
    }

    void fillSingletons(Module &M) {
        for (auto interesting_t : interesting_types) {
            // Dummy object is already created for this type
            for (size_t i = 0; i < interesting_t->getNumElements(); i++) {
                auto field = interesting_t->getElementType(i);
                if (isFunctionPointer(field)) {
                    createStubFunction(M, interesting_t, i);
                    // add initializer
                    initializeStructureField(M, interesting_t, i);
                }
                // TODO: substructure and pointer to structure
            }
        }
    }

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


    void initializeStructureField(Module &M, StructType *T, size_t field_idx) {
        LLVMContext &ctx = M.getContext();
        IRBuilder<> builder(ctx);
        builder.SetInsertPoint(global_initializer_bb);

        if (dereferenceFPtr(T->getTypeAtIndex(field_idx))) {
            Value *ptr_gep = builder.CreateStructGEP(T, singletons[T], field_idx);
            builder.CreateStore(
                function_stubs[{T, field_idx}],
                ptr_gep
            );
        }
        // TODO: substructure and pointer to structure
    }

    // Create a singleton with a given type and no fields
    GlobalVariable* createSingleton(Module &M, StructType *T) {
        if (singletons.contains(T)) {
            return singletons[T];
        }
        // Mark is as external to avoid initializing structure
        auto *var = new GlobalVariable(
                M, T, false, GlobalValue::ExternalLinkage, nullptr,
                structSingletonName(T->getName().operator std::string())
        );
        singletons[T] = var;
        return var;
    }

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

        Value *struct_ptr = builder.CreateLoad(T->getPointerTo(), singletons[T]);
        Value *ptr_gep = builder.CreateStructGEP(T, struct_ptr, field_idx);
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

    void inspectStructs(Module &M) {
        std::unordered_map<StructType*, std::set<StructType*>> backlinks;
        std::vector<StructType*> work_list;

        for (auto s : M.getIdentifiedStructTypes()) {
            bool was_interesting = false;
            for (auto field : s->elements()) {
                if (isFunctionPointer(field)) {
                    // This struct contains a pointer to a function
                    interesting_types.insert(s);
                    was_interesting = true;
                } else if (auto *substruct = dyn_cast<StructType>(field)) {
                    // Add backlink to the subfield
                    backlinks[substruct].insert(s);
                }
                // TODO: pointer to structure
            }
            if (was_interesting) {
                // This is a base interesting structure
                work_list.push_back(s);
                // outs() << "Struct " << s->getName() << " marked interesting\n";
            }
        }
        while (!work_list.empty()) {
            auto s = work_list.back();
            work_list.pop_back();
            for (auto par : backlinks[s]) {
                if (interesting_types.insert(par).second) {
                    work_list.push_back(par);
                    outs() << "Struct " << par->getName() << " marked interesting: ";
                    outs() << par->getName() << " -> " << s->getName() << "\n";
                }
            }
        }
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
