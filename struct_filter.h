#pragma once

#include <set>
#include <unordered_set>
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
    Module *M = nullptr;
    /// T { G, *F } leads to T -> G and T -> F edges.
    std::unordered_map<StructType*, std::unordered_set<StructType*>> type_graph;
    std::unordered_map<StructType*, std::unordered_set<StructType*>> inv_type_graph;
    std::set<StructType*> interesting_types;

    void findInterestingStructs();

    void buildTypeGraph();

    void markChildrenUsed(StructType *t, std::unordered_set<StructType*> &dfs_used);

    void markParentsUsed(StructType *t, std::unordered_set<StructType*> &dfs_used);
};
