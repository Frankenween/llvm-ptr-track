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
                interesting_types.insert(s);
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
        if (auto *t = dyn_cast<StructType>(glob.getType())) {
            markChildrenUsed(t, used_structs);
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
