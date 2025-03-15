#include "struct_filter.h"
#include "util.h"

struct_filter::struct_filter(Module *M): M(M) {
    buildTypeGraph();
    findInterestingStructs();
}

bool struct_filter::isInterestingType(Type *t) {
    if (auto *ret = dyn_cast<StructType>(t)) {
        return interesting_types.contains(ret);
    }
    return false;
}

bool struct_filter::isPtrToInterestingType(Type *t) {
    return t->isPointerTy() && isInterestingType(t->getNonOpaquePointerElementType());
}

// Check is this is an interesting type or a pointer to an interesting type
bool struct_filter::isInterestingTypeOrPtr(Type *t) {
    return isInterestingType(t) || isPtrToInterestingType(t);
}

// Iterate over all structures and find structs with function pointers
// After in find all structures, that contain interesting fields(as structs or pointers)
void struct_filter::findInterestingStructs() {
    // used_struct may contain nullptr
    std::unordered_set<StructType*> used_structs, interesting;

    for (auto s : M->getIdentifiedStructTypes()) {
        for (auto field : s->elements()) {
            if (isFunctionPointer(field)) {
                // This struct contains a pointer to a function
                markParentsUsed(s, interesting);
                break;
            }
        }
    }
    // Now we have all interesting structs

    // Globals should be marked used with all field
    // T { G } may have G defined but never accessed from any function
    // in this case T is passed to an external function, which will access G
    for (auto &glob : M->getGlobalList()) {
        if (!glob.hasInitializer() || glob.isNullValue()) {
            continue;
        }
        auto *initializer = glob.getInitializer();
        if (!initializer || initializer->isZeroValue() || initializer->isNullValue()) {
            continue;
        }
        if (dyn_cast<StructType>(glob.getValueType())) {
            auto *init_struct = dyn_cast<ConstantStruct>(initializer);
            if (!init_struct) {
                continue;
            }
            markUsedGlobalRecursively(init_struct, used_structs);
        } else if (auto *arr_ty = dyn_cast<ArrayType>(glob.getValueType())) {
            auto *init_arr = dyn_cast<ConstantArray>(glob.getInitializer());
            auto *el_ty = dyn_cast<StructType>(arr_ty->getElementType());
            if (!init_arr || !el_ty) {
                continue;
            }
            for (size_t i = 0; i < initializer->getNumOperands(); i++) {
                auto *el = dyn_cast<ConstantStruct>(initializer->getOperand(i));
                if (!el) {
                    // It is a zero initializer(usually as a marker of array end)
                    continue;
                }
                markUsedGlobalRecursively(el, used_structs);
            }
        }
    }
    // Get function arguments
    for (auto &f : M->getFunctionList()) {
        // Return value
        used_structs.insert(getStructType(f.getReturnType()));
        for (auto *arg : f.getFunctionType()->params()) {
            used_structs.insert(getStructType(arg));
        }
    }
    // Get all used types, used in instructions
    for (auto &f : *M) {
        for (auto &bb : f) {
            for (auto &inst : bb) {
                if (auto *cast = dyn_cast<BitCastInst>(&inst)) {
                    used_structs.insert(getStructType(cast->getSrcTy()));
                    used_structs.insert(getStructType(cast->getDestTy()));
                } else if (auto *gep = dyn_cast<GetElementPtrInst>(&inst)) {
                    used_structs.insert(getStructType(gep->getSourceElementType()));
                    used_structs.insert(getStructType(gep->getResultElementType()));
                } else if (auto *i2p = dyn_cast<IntToPtrInst>(&inst)) {
                    used_structs.insert(getStructType(i2p->getDestTy()));
                } else if (auto *load = dyn_cast<LoadInst>(&inst)) {
                    used_structs.insert(getStructType(load->getPointerOperandType()));
                } else if (auto *store = dyn_cast<StoreInst>(&inst)) {
                    used_structs.insert(getStructType(store->getPointerOperandType()));
                }
            }
        }
    }
    for (auto *i_s : interesting) {
        if (used_structs.contains(i_s)) {
            interesting_types.insert(i_s);
            //outs() << i_s->getName() << " is interesting!\n";
        } else {
            //outs() << "Dropping " << i_s->getName() << " as unused\n";
        }
    }
    //outs().flush();
}

void struct_filter::markUsedGlobalRecursively(ConstantStruct *val, std::unordered_set<StructType*> &used) {
    auto *t = val->getType();
    // TODO: if there are two globals with the same type, but different initialized fields, one will be skipped
    if (used.contains(t)) {
        return;
    }
    used.insert(t);
    for (size_t i = 0; i < val->getNumOperands(); i++) {
        if (getStructType(t->getTypeAtIndex(i))) {
            auto *field_init = dyn_cast<ConstantStruct>(val->getOperand(i));
            if (field_init && !field_init->isNullValue()) {
                markUsedGlobalRecursively(field_init, used);
            }
        }
    }
}

void struct_filter::buildTypeGraph() {
    for (auto s : M->getIdentifiedStructTypes()) {
        for (auto field : s->elements()) {
            if (auto *field_type = getStructType(field)) {
                type_graph[s].insert(field_type);
                inv_type_graph[field_type].insert(s);
            }
        }
    }
}

void struct_filter::markChildrenUsed(StructType *t, std::unordered_set<StructType*> &dfs_used) {
    if (dfs_used.contains(t)) {
        return;
    }
    dfs_used.insert(t);
    for (auto child : type_graph[t]) {
        markChildrenUsed(child, dfs_used);
    }
}

void struct_filter::markParentsUsed(StructType *t, std::unordered_set<StructType*> &dfs_used) {
    if (dfs_used.contains(t)) {
        return;
    }
    dfs_used.insert(t);
    for (auto parent : inv_type_graph[t]) {
        markParentsUsed(parent, dfs_used);
    }
}

const std::set<StructType *> &struct_filter::getInterestingTypes() const {
    return interesting_types;
}
