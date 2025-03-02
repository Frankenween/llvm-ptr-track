#include <unordered_set>
#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "util.h"

using namespace llvm;

// TODO: read list of structures from a file
std::unordered_set<std::string> NO_PTR_STORE = {
        "struct.list_head",
        "struct.hlist_node",
        "struct.llist_node",
};

struct StorePurgerPass : public ModulePass {
    StorePurgerPass(): ModulePass(ID) {}

    bool runOnModule(Module &M) override {
        restricted_for_store = findAllStructsByName(M, NO_PTR_STORE);
        removeAllStores(M);
        return false;
    }
    static char ID;
private:
    std::unordered_set<Type*> restricted_for_store;

    void removeAllStores(Module &M) {
        size_t purged = 0;
        for (auto &f : M) {
            for (auto &bb : f) {
                std::vector<Instruction*> remove_list;
                for (auto &inst : bb) {
                    if (auto *store = dyn_cast<StoreInst>(&inst)) {
                        auto *type = dereferenceStructPtr(store->getValueOperand()->getType());
                        if (!type || !restricted_for_store.contains(type)) {
                            continue;
                        }
                        purged++;
                        remove_list.push_back(store);
                        //store->eraseFromParent();
                    }
                }
                for (auto i : remove_list) {
                    i->eraseFromParent();
                }
            }
        }
        outs() << "Removed " << purged << " stores\n";
        outs().flush();
    }
};

char StorePurgerPass::ID = 0;

static RegisterPass<StorePurgerPass> X("remove-store", "Remove unwanted stores",
                                         false /* Only looks at CFG */,
                                         false /* Analysis Pass */);

static RegisterStandardPasses Y(
        PassManagerBuilder::EP_OptimizerLast,
        [](const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
            PM.add(new StorePurgerPass());
        }
);
