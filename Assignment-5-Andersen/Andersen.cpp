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
    
    // 初始化: 处理所有 Address 约束
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
        SVF::NodeID nodeId = worklist.pop();
        SVF::ConstraintNode* node = consg->getConstraintNode(nodeId);
        const std::set<unsigned>& nodePts = pts[nodeId];
        
        // Copy 边: p = q
        for (auto edge : node->getCopyOutEdges()) {
            SVF::NodeID dstId = edge->getDstID();
            size_t oldSize = pts[dstId].size();
            pts[dstId].insert(nodePts.begin(), nodePts.end());
            if (pts[dstId].size() > oldSize) {
                worklist.push(dstId);
            }
        }
        
        // Load 边: p = *q
        for (auto edge : node->getLoadOutEdges()) {
            SVF::NodeID dstId = edge->getDstID();
            size_t oldSize = pts[dstId].size();
            for (SVF::NodeID o : nodePts) {
                pts[dstId].insert(pts[o].begin(), pts[o].end());
            }
            if (pts[dstId].size() > oldSize) {
                worklist.push(dstId);
            }
        }
        
        // Store 边: *p = q
        for (auto edge : node->getStoreOutEdges()) {
            SVF::NodeID srcId = edge->getSrcID();
            for (SVF::NodeID o : nodePts) {
                size_t oldSize = pts[o].size();
                pts[o].insert(pts[srcId].begin(), pts[srcId].end());
                if (pts[o].size() > oldSize) {
                    worklist.push(o);
                }
            }
        }
        
        // GEP 边: p = &q->field
        // 关键修复: 使用 getGepObjVar(NodeID, const GepCGEdge*)
        for (auto edge : node->getGepOutEdges()) {
            SVF::NodeID dstId = edge->getDstID();
            bool changed = false;
            
            for (SVF::NodeID o : nodePts) {
                // 直接传入 edge,让 SVF 内部处理偏移计算
                SVF::NodeID fieldObj = consg->getGepObjVar(o, edge);
                if (pts[dstId].insert(fieldObj).second) {
                    changed = true;
                }
            }
            
            if (changed) {
                worklist.push(dstId);
            }
        }
    }
}
