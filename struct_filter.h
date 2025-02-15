#pragma once

#include <set>
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

class struct_filter {
public:
    struct_filter() = default;

    explicit struct_filter(Module *M);

    bool isInterestingType(Type *t);

    bool isPtrToInterestingType(Type *t);

    // Check is this is an interesting type or a pointer to an interesting type
    bool isInterestingTypeOrPtr(Type *t);

    [[nodiscard]] const std::set<StructType*>& getInterestingTypes() const;
private:
    Module *M;
    std::set<StructType*> interesting_types;

    void findInterestingStructs();
};
