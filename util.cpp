#include "util.h"

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

StructType* dereferenceStructPtr(Type *t) {
    if (auto ptr = dyn_cast<PointerType>(t)) {
        return dyn_cast<StructType>(t->getNonOpaquePointerElementType());
    }
    return nullptr;
}

Value* copyStructBetweenPointers(Module &M, IRBuilder<> &builder, Type* T, Value* src, Value* dst) {
    return builder.CreateMemCpy(
            dst,
            MaybeAlign(),
            src,
            MaybeAlign(),
            M.getDataLayout().getTypeAllocSize(T)
    );
}