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
    
    // Step 1: Address 规则 (o --Addr--> p)
    for (auto iter = consg->begin(); iter != consg->end(); ++iter) {
        SVF::ConstraintNode* node = iter->second;
        SVF::NodeID objId = node->getId();  // 当前节点作为对象
        
        // 遍历从对象出发的 Address 出边
        for (auto edge : node->getAddrOutEdges()) {
            SVF::NodeID ptrId = edge->getDstID();
            if (pts[ptrId].insert(objId).second) {
                worklist.push(ptrId);
            }
        }
    }
    
    // Step 2: 主循环
    while (!worklist.empty()) {
        SVF::NodeID p = worklist.pop();
        SVF::ConstraintNode* nodeP = consg->getConstraintNode(p);
        
        // Store 规则: *p = q (需要动态添加 q->o 的 Copy 边)
        for (auto edge : nodeP->getStoreInEdges()) {
            SVF::NodeID q = edge->getSrcID();
            
            for (SVF::NodeID o : pts[p]) {
                SVF::ConstraintNode* qNode = consg->getConstraintNode(q);
                SVF::ConstraintNode* oNode = consg->getConstraintNode(o);
                
                if (!consg->hasEdge(qNode, oNode, SVF::ConstraintEdge::Copy)) {
                    consg->addCopyCGEdge(q, o);
                    worklist.push(q);
                }
            }
        }
        
        // Load 规则: r = *p (需要动态添加 o->r 的 Copy 边)
        for (auto edge : nodeP->getLoadOutEdges()) {
            SVF::NodeID r = edge->getDstID();
            
            for (SVF::NodeID o : pts[p]) {
                SVF::ConstraintNode* oNode = consg->getConstraintNode(o);
                SVF::ConstraintNode* rNode = consg->getConstraintNode(r);
                
                if (!consg->hasEdge(oNode, rNode, SVF::ConstraintEdge::Copy)) {
                    consg->addCopyCGEdge(o, r);
                    worklist.push(o);
                }
            }
        }
        
        // Copy 规则: p = q
        for (auto edge : nodeP->getCopyOutEdges()) {
            SVF::NodeID x = edge->getDstID();
            size_t oldSize = pts[x].size();
            pts[x].insert(pts[p].begin(), pts[p].end());
            if (pts[x].size() > oldSize) {
                worklist.push(x);
            }
        }
        
        // Gep 规则: x = &p->field
        for (auto edge : nodeP->getGepOutEdges()) {
            // 转换为 NormalGepCGEdge 获取常量偏移
            SVF::NormalGepCGEdge* gepEdge = SVF::SVFUtil::dyn_cast<SVF::NormalGepCGEdge>(edge);
            if (!gepEdge) continue;
            
            SVF::NodeID x = gepEdge->getDstID();
            SVF::APOffset offset = gepEdge->getConstantFieldIdx();
            
            size_t oldSize = pts[x].size();
            for (SVF::NodeID o : pts[p]) {
                SVF::NodeID fieldObj = consg->getGepObjVar(o, offset);
                pts[x].insert(fieldObj);
            }
            
            if (pts[x].size() > oldSize) {
                worklist.push(x);
            }
        }
    }
}
