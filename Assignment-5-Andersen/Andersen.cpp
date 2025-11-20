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
    WorkList<unsigned> wl;
    
    // 初始化阶段：处理 Address 约束
    for (auto nodeIter = consg->begin(); nodeIter != consg->end(); ++nodeIter) {
        SVF::ConstraintNode *curNode = nodeIter->second;
        
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& addrEdges = curNode->getAddrInEdges();
        for (SVF::ConstraintEdge* edge : addrEdges) {
            SVF::AddrCGEdge *ae = SVF::SVFUtil::dyn_cast<SVF::AddrCGEdge>(edge);
            if (ae) {
                unsigned src = ae->getSrcID();
                unsigned dst = ae->getDstID();
                pts[dst].insert(src);
                wl.push(dst);
            }
        }
    }
    
    // 迭代求解
    while (!wl.empty()) {
        unsigned nodeId = wl.pop();
        SVF::ConstraintNode *node = consg->getConstraintNode(nodeId);
        
        // 获取当前节点的 pts
        const std::set<unsigned>& currentPts = pts[nodeId];
        
        // 处理 Store 和 Load 规则 - 需要遍历 pts 中的每个对象
        for (unsigned obj : currentPts) {
            // Store 规则: *nodeId = src, 为每个 obj 添加 src->obj 的 Copy 边
            const SVF::ConstraintEdge::ConstraintEdgeSetTy& storeEdges = node->getStoreInEdges();
            for (SVF::ConstraintEdge* e : storeEdges) {
                SVF::StoreCGEdge *se = SVF::SVFUtil::dyn_cast<SVF::StoreCGEdge>(e);
                if (!se) continue;
                
                unsigned srcNode = se->getSrcID();
                SVF::ConstraintNode *srcCNode = consg->getConstraintNode(srcNode);
                
                // 检查 srcNode -> obj 的 Copy 边是否已存在
                bool edgeExists = false;
                for (SVF::ConstraintEdge* copyE : srcCNode->getCopyOutEdges()) {
                    if (copyE->getDstID() == obj) {
                        edgeExists = true;
                        break;
                    }
                }
                
                if (!edgeExists) {
                    consg->addCopyCGEdge(srcNode, obj);
                    wl.push(srcNode);
                }
            }
            
            // Load 规则: dst = *nodeId, 为每个 obj 添加 obj->dst 的 Copy 边
            const SVF::ConstraintEdge::ConstraintEdgeSetTy& loadEdges = node->getLoadOutEdges();
            for (SVF::ConstraintEdge* e : loadEdges) {
                SVF::LoadCGEdge *le = SVF::SVFUtil::dyn_cast<SVF::LoadCGEdge>(e);
                if (!le) continue;
                
                unsigned dstNode = le->getDstID();
                SVF::ConstraintNode *dstCNode = consg->getConstraintNode(dstNode);
                
                // 检查 obj -> dstNode 的 Copy 边是否已存在
                bool edgeExists = false;
                for (SVF::ConstraintEdge* copyE : dstCNode->getCopyInEdges()) {
                    if (copyE->getSrcID() == obj) {
                        edgeExists = true;
                        break;
                    }
                }
                
                if (!edgeExists) {
                    consg->addCopyCGEdge(obj, dstNode);
                    wl.push(obj);
                }
            }
        }
        
        // Copy 规则: 直接传播 pts
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& copyEdges = node->getCopyOutEdges();
        for (SVF::ConstraintEdge* e : copyEdges) {
            SVF::CopyCGEdge *ce = SVF::SVFUtil::dyn_cast<SVF::CopyCGEdge>(e);
            if (!ce) continue;
            
            unsigned target = ce->getDstID();
            size_t prevSize = pts[target].size();
            pts[target].insert(currentPts.begin(), currentPts.end());
            
            if (pts[target].size() > prevSize) {
                wl.push(target);
            }
        }
        
        // Gep 规则: 获取字段对象
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& gepEdges = node->getGepOutEdges();
        for (SVF::ConstraintEdge* e : gepEdges) {
            SVF::GepCGEdge *ge = SVF::SVFUtil::dyn_cast<SVF::GepCGEdge>(e);
            if (!ge) continue;
            
            unsigned target = ge->getDstID();
            size_t prevSize = pts[target].size();
            
            for (unsigned obj : currentPts) {
                unsigned fieldNode = consg->getGepObjVar(obj, ge);
                pts[target].insert(fieldNode);
            }
            
            if (pts[target].size() > prevSize) {
                wl.push(target);
            }
        }
    }
}
