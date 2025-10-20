// /**
//  * Andersen.cpp
//  * @author kisslune
//  */

// #include "A5Header.h"

// using namespace llvm;
// using namespace std;

// int main(int argc, char** argv)
// {
//     auto moduleNameVec =
//             OptionBase::parseOptions(argc, argv, "Whole Program Points-to Analysis",
//                                      "[options] <input-bitcode...>");

//     SVF::LLVMModuleSet::buildSVFModule(moduleNameVec);

//     SVF::SVFIRBuilder builder;
//     auto pag = builder.build();
//     auto consg = new SVF::ConstraintGraph(pag);
//     consg->dump();

//     Andersen andersen(consg);

//     // TODO: complete the following method
//     andersen.runPointerAnalysis();

//     andersen.dumpResult();
//     SVF::LLVMModuleSet::releaseLLVMModuleSet();
// 	return 0;
// }


// void Andersen::runPointerAnalysis()
// {
//     // TODO: complete this method. Point-to set and worklist are defined in A5Header.h
//     //  The implementation of constraint graph is provided in the SVF library
// }

/**
 * Andersen.cpp
 * @author kisslune
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
    //consg->dump();

    Andersen andersen(consg);

    // TODO: complete the following method
    andersen.runPointerAnalysis();

    andersen.dumpResult();
    SVF::LLVMModuleSet::releaseLLVMModuleSet();
	return 0;
}


// void Andersen::runPointerAnalysis()
// {
//     // TODO: complete this method. Point-to set and worklist are defined in A5Header.h
//     //  The implementation of constraint graph is provided in the SVF library
// }
void Andersen::runPointerAnalysis()
{
    // 工作列表：存储需要处理的节点
    WorkList<SVF::NodeID> worklist;
    
    // Step 1: 初始化 - 处理所有的 Address 约束 (p = &a)
    for (auto iter = consg->begin(); iter != consg->end(); ++iter) {
        SVF::NodeID nodeId = iter->first;
        SVF::ConstraintNode* node = iter->second;
        
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& addrInEdges = node->getAddrInEdges();
        
        for (auto edge : addrInEdges) {
            SVF::NodeID srcId = edge->getSrcID();
            SVF::NodeID dstId = edge->getDstID();
            
            if (pts[dstId].insert(srcId).second) {
                worklist.push(dstId);
            }
        }
    }
    
    // Step 2: 主循环
    while (!worklist.empty()) {
        SVF::NodeID nodeId = worklist.pop();
        SVF::ConstraintNode* node = consg->getConstraintNode(nodeId);
        const std::set<unsigned>& nodePts = pts[nodeId];
        
        // Step 2.1: Copy 边
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& copyOutEdges = node->getCopyOutEdges();
        for (auto edge : copyOutEdges) {
            SVF::NodeID dstId = edge->getDstID();
            size_t oldSize = pts[dstId].size();
            pts[dstId].insert(nodePts.begin(), nodePts.end());
            if (pts[dstId].size() > oldSize) {
                worklist.push(dstId);
            }
        }
        
        // Step 2.2: Load 边
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& loadOutEdges = node->getLoadOutEdges();
        for (auto edge : loadOutEdges) {
            SVF::LoadCGEdge* loadEdge = SVF::SVFUtil::dyn_cast<SVF::LoadCGEdge>(edge);
            if (!loadEdge) continue;
            
            SVF::NodeID dstId = loadEdge->getDstID();
            for (SVF::NodeID o : nodePts) {
                size_t oldSize = pts[dstId].size();
                pts[dstId].insert(pts[o].begin(), pts[o].end());
                if (pts[dstId].size() > oldSize) {
                    worklist.push(dstId);
                }
            }
        }
        
        // Step 2.3: Store 边
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& storeOutEdges = node->getStoreOutEdges();
        for (auto edge : storeOutEdges) {
            SVF::StoreCGEdge* storeEdge = SVF::SVFUtil::dyn_cast<SVF::StoreCGEdge>(edge);
            if (!storeEdge) continue;
            
            SVF::NodeID srcId = storeEdge->getSrcID();
            const std::set<unsigned>& srcPts = pts[srcId];
            
            for (SVF::NodeID o : nodePts) {
                size_t oldSize = pts[o].size();
                pts[o].insert(srcPts.begin(), srcPts.end());
                if (pts[o].size() > oldSize) {
                    worklist.push(o);
                }
            }
        }
        
        // Step 2.4: Gep 边
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& gepOutEdges = node->getGepOutEdges();
        for (auto edge : gepOutEdges) {
            SVF::GepCGEdge* gepEdge = SVF::SVFUtil::dyn_cast<SVF::GepCGEdge>(edge);
            if (!gepEdge) continue;
            
            SVF::NodeID dstId = gepEdge->getDstID();
            for (SVF::NodeID o : nodePts) {
                if (pts[dstId].insert(o).second) {
                    worklist.push(dstId);
                }
            }
        }
    }
}


