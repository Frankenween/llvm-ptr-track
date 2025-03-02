#pragma once
#include <unordered_set>
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

// Check if it is a function pointer type
bool isFunctionPointer(Type *t);

// Get function type from a pointer to it
FunctionType* dereferenceFPtr(Type *t);

StructType* dereferenceStructPtr(Type *t);

Value* copyStructBetweenPointers(Module &M, IRBuilder<> &builder, Type* T, Value* src, Value* dst);

std::unordered_set<Type*> findAllStructsByName(Module &M, const std::unordered_set<std::string> &names);
