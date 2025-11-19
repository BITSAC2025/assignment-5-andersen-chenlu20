/**
 * Andersen.cpp
 * @author 3220252746
 */
#include "A5Header.h"

using namespace llvm;
using namespace std;

int main(int argc, char** argv)
{
    auto moduleNameVec =
            OptionBase::parseOptions(argc, argv, "Whole Program Points-to Analysis",
                                     "[options] <input-bitcode...>");
    SVF::LLVMModuleSet::buildSVFModule(moduleNameVec);
    SVF::SVFIRBuilder builder;
    auto pag = builder.build();
    auto consg = new SVF::ConstraintGraph(pag);
    
    Andersen andersen(consg);
    andersen.runPointerAnalysis();
    andersen.dumpResult();
    
    SVF::LLVMModuleSet::releaseLLVMModuleSet();
    return 0;
}

void Andersen::runPointerAnalysis()
{
    WorkList<SVF::NodeID> worklist;
    
    // 初始化
    for (auto iter = consg->begin(); iter != consg->end(); ++iter) {
        SVF::ConstraintNode* node = iter->second;
        for (auto edge : node->getAddrInEdges()) {
            if (pts[edge->getDstID()].insert(edge->getSrcID()).second) {
                worklist.push(edge->getDstID());
            }
        }
    }
    
    // 主循环
    while (!worklist.empty()) {
        SVF::NodeID n = worklist.pop();
        SVF::ConstraintNode* node = consg->getConstraintNode(n);
        
        // Copy
        for (auto e : node->getCopyOutEdges()) {
            size_t old = pts[e->getDstID()].size();
            pts[e->getDstID()].insert(pts[n].begin(), pts[n].end());
            if (pts[e->getDstID()].size() > old) {
                worklist.push(e->getDstID());
            }
        }
        
        // Load
        for (auto e : node->getLoadOutEdges()) {
            size_t old = pts[e->getDstID()].size();
            for (auto o : pts[n]) {
                pts[e->getDstID()].insert(pts[o].begin(), pts[o].end());
            }
            if (pts[e->getDstID()].size() > old) {
                worklist.push(e->getDstID());
            }
        }
        
        // Store
        for (auto e : node->getStoreOutEdges()) {
            for (auto o : pts[n]) {
                size_t old = pts[o].size();
                pts[o].insert(pts[e->getSrcID()].begin(), pts[e->getSrcID()].end());
                if (pts[o].size() > old) {
                    worklist.push(o);
                }
            }
        }
        
        // Gep - 使用最简单的方式
        for (auto e : node->getGepOutEdges()) {
            bool changed = false;
            for (auto o : pts[n]) {
                if (pts[e->getDstID()].insert(consg->getGepObjVar(o, 0)).second) {
                    changed = true;
                }
            }
            if (changed) {
                worklist.push(e->getDstID());
            }
        }
    }
}
