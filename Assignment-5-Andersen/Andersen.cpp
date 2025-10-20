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
    // 遍历约束图中的所有节点
    for (auto iter = consg->begin(); iter != consg->end(); ++iter) {
        SVF::NodeID nodeId = iter->first;
        SVF::ConstraintNode* node = iter->second;
        
        // 获取该节点的所有 Address 输入边
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& addrInEdges = node->getAddrInEdges();
        
        // 处理每条 Address 边：p = &a
        for (auto edge : addrInEdges) {
            SVF::NodeID srcId = edge->getSrcID();  // a 的ID
            SVF::NodeID dstId = edge->getDstID();  // p 的ID
            
            // 将 srcId 加入到 dstId 的指向集合
            if (pts[dstId].insert(srcId).second) {
                // 如果指向集合发生了变化，将节点加入工作列表
                worklist.push(dstId);
            }
        }
    }
    
    // Step 2: 主循环 - 使用工作列表算法传播指向信息
    while (!worklist.empty()) {
        SVF::NodeID nodeId = worklist.pop();
        
        // 获取当前节点
        SVF::ConstraintNode* node = consg->getConstraintNode(nodeId);
        
        // 获取当前节点的指向集合
        const std::set<unsigned>& nodePts = pts[nodeId];
        
        // Step 2.1: 处理 Copy 边 (p = q)
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& copyOutEdges = node->getCopyOutEdges();
        for (auto edge : copyOutEdges) {
            SVF::NodeID dstId = edge->getDstID();  // p 的ID
            
            // 将 q 的指向集合传播到 p
            size_t oldSize = pts[dstId].size();
            pts[dstId].insert(nodePts.begin(), nodePts.end());
            
            // 如果 p 的指向集合发生变化，将 p 加入工作列表
            if (pts[dstId].size() > oldSize) {
                worklist.push(dstId);
            }
        }
        
        // Step 2.2: 处理 Load 边 (p = *q)
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& loadOutEdges = node->getLoadOutEdges();
        for (auto edge : loadOutEdges) {
            SVF::LoadCGEdge* loadEdge = SVF::SVFUtil::dyn_cast<SVF::LoadCGEdge>(edge);
            if (!loadEdge) continue;
            
            SVF::NodeID dstId = loadEdge->getDstID();  // p 的ID
            
            // 对于 q 指向的每个对象 o
            for (SVF::NodeID o : nodePts) {
                // 获取 o 的指向集合，并将其并入 p 的指向集合
                size_t oldSize = pts[dstId].size();
                pts[dstId].insert(pts[o].begin(), pts[o].end());
                
                if (pts[dstId].size() > oldSize) {
                    worklist.push(dstId);
                }
            }
        }
        
        // Step 2.3: 处理 Store 边 (*p = q)
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& storeOutEdges = node->getStoreOutEdges();
        for (auto edge : storeOutEdges) {
            SVF::StoreCGEdge* storeEdge = SVF::SVFUtil::dyn_cast<SVF::StoreCGEdge>(edge);
            if (!storeEdge) continue;
            
            SVF::NodeID srcId = storeEdge->getSrcID();  // q 的ID
            
            // 获取 q 的指向集合
            const std::set<unsigned>& srcPts = pts[srcId];
            
            // 对于当前节点(p)指向的每个对象 o
            for (SVF::NodeID o : nodePts) {
                // *p = q, p -> o, 所以将 q 的指向集合并入 o 的指向集合
                size_t oldSize = pts[o].size();
                pts[o].insert(srcPts.begin(), srcPts.end());
                
                if (pts[o].size() > oldSize) {
                    worklist.push(o);
                }
            }
        }
        
        // Step 2.4: 处理 Gep (getelementptr) 边
        // Step 2.4: 处理 Gep (getelementptr) 边
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& gepOutEdges = node->getGepOutEdges();
        for (auto edge : gepOutEdges) {
            SVF::GepCGEdge* gepEdge = SVF::SVFUtil::dyn_cast<SVF::GepCGEdge>(edge);
            if (!gepEdge) continue;
            
            SVF::NodeID dstId = gepEdge->getDstID();
            
            // 对于 pts 中的每个对象 o
            for (SVF::NodeID o : nodePts) {
                // 简化处理：直接将 o 加入 dstId 的指向集合
                // 在基本的 Andersen 分析中，可以不区分字段
                if (pts[dstId].insert(o).second) {
                    worklist.push(dstId);
          
        
                }
            }
        }
    }
}
