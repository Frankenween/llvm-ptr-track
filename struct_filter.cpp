#include "struct_filter.h"
#include "util.h"

struct_filter::struct_filter(Module *M): M(M) {
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
    std::unordered_map<StructType*, std::set<StructType*>> backlinks;
    std::vector<StructType*> work_list;

    for (auto s : M->getIdentifiedStructTypes()) {
        bool was_interesting = false;
        for (auto field : s->elements()) {
            if (isFunctionPointer(field)) {
                // This struct contains a pointer to a function
                interesting_types.insert(s);
                was_interesting = true;
            } else if (auto *substruct = dyn_cast<StructType>(field)) {
                // Add backlink to the subfield
                backlinks[substruct].insert(s);
            } else if (auto *substruct_ptr = dyn_cast<PointerType>(field)) {
                if (auto *par = dyn_cast<StructType>(substruct_ptr->getNonOpaquePointerElementType())) {
                    backlinks[par].insert(s);
                }
            }
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

const std::set<StructType *> &struct_filter::getInterestingTypes() const {
    return interesting_types;
}
