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
 * Assignment-5: Andersen's Pointer Analysis Algorithm
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
    //consg->dump();  // 注释掉以避免影响输出

    Andersen andersen(consg);

    // 运行指针分析
    andersen.runPointerAnalysis();

    andersen.dumpResult();
    SVF::LLVMModuleSet::releaseLLVMModuleSet();
	return 0;
}

void Andersen::runPointerAnalysis()
{
    // 实现Andersen算法
    // 算法的核心是处理五种约束边：Address, Copy, Load, Store, Gep
    
    // 初始化工作列表
    WorkList<SVF::NodeID> worklist;
    
    // Step 1: 初始化阶段
    // 处理所有的Address边 (p = &a)
    // 遍历约束图中的所有节点
    for (auto iter = consg->begin(); iter != consg->end(); ++iter) {
        SVF::ConstraintNode* node = iter->second;
        SVF::NodeID nodeId = node->getId();
        
        // 获取该节点的所有Address入边
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& addrInEdges = node->getAddrInEdges();
        
        // 对于每条Address边，将源节点（对象）加入目标节点（指针）的pts
        for (auto edge : addrInEdges) {
            SVF::NodeID srcId = edge->getSrcID();  // 对象节点
            
            // 如果成功添加了新元素，将节点加入工作列表
            if (pts[nodeId].insert(srcId).second) {
                worklist.push(nodeId);
            }
        }
    }
    
    // Step 2: 迭代传播阶段
    // 使用工作列表算法处理约束传播
    while (!worklist.empty()) {
        // 从工作列表中取出一个节点
        SVF::NodeID nodeId = worklist.pop();
        SVF::ConstraintNode* node = consg->getConstraintNode(nodeId);
        
        // 获取当前节点的pts集合
        const std::set<unsigned>& currentPts = pts[nodeId];
        
        // 2.1 处理Copy边 (q = p)
        // pts(p) ⊆ pts(q)
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& copyOutEdges = node->getCopyOutEdges();
        for (auto edge : copyOutEdges) {
            SVF::NodeID dstId = edge->getDstID();
            
            // 记录原始大小
            size_t originalSize = pts[dstId].size();
            
            // 将currentPts中的所有元素添加到目标节点
            pts[dstId].insert(currentPts.begin(), currentPts.end());
            
            // 如果pts发生变化，将目标节点加入工作列表
            if (pts[dstId].size() > originalSize) {
                worklist.push(dstId);
            }
        }
        
        // 2.2 处理Load边 (p = *q)
        // ∀o ∈ pts(q): pts(o) ⊆ pts(p)
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& loadOutEdges = node->getLoadOutEdges();
        for (auto edge : loadOutEdges) {
            SVF::NodeID dstId = edge->getDstID();
            
            // 记录原始大小
            size_t originalSize = pts[dstId].size();
            
            // 对于q指向的每个对象o
            for (SVF::NodeID objectId : currentPts) {
                // 将pts(o)的内容添加到pts(p)
                const std::set<unsigned>& objectPts = pts[objectId];
                pts[dstId].insert(objectPts.begin(), objectPts.end());
            }
            
            // 如果pts发生变化，将目标节点加入工作列表
            if (pts[dstId].size() > originalSize) {
                worklist.push(dstId);
            }
        }
        
        // 2.3 处理Store边 (*p = q)
        // ∀o ∈ pts(p): pts(q) ⊆ pts(o)
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& storeOutEdges = node->getStoreOutEdges();
        for (auto edge : storeOutEdges) {
            SVF::NodeID srcId = edge->getSrcID();  // q节点
            const std::set<unsigned>& srcPts = pts[srcId];
            
            // 对于p指向的每个对象o
            for (SVF::NodeID objectId : currentPts) {
                // 记录原始大小
                size_t originalSize = pts[objectId].size();
                
                // 将pts(q)的内容添加到pts(o)
                pts[objectId].insert(srcPts.begin(), srcPts.end());
                
                // 如果pts发生变化，将对象节点加入工作列表
                if (pts[objectId].size() > originalSize) {
                    worklist.push(objectId);
                }
            }
        }
        
        // 2.4 处理Gep边 (p = &q->field)
        // ∀o ∈ pts(q): o.field ∈ pts(p)
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& gepOutEdges = node->getGepOutEdges();
        for (auto edge : gepOutEdges) {
            // 转换为GepCGEdge类型
            SVF::GepCGEdge* gepEdge = SVF::SVFUtil::dyn_cast<SVF::GepCGEdge>(edge);
            if (!gepEdge) continue;
            
            SVF::NodeID dstId = gepEdge->getDstID();
            bool changed = false;
            
            // 获取字段偏移量
            SVF::APOffset offset = 0;
            if (gepEdge->isConstantOffset()) {
                // 对于常量偏移，获取具体的偏移值
                offset = gepEdge->getAccessPath().getConstantOffset();
            }
            
            // 对于q指向的每个对象o
            for (SVF::NodeID objectId : currentPts) {
                // 获取对象o的字段field对应的节点
                SVF::NodeID fieldObjId = consg->getGepObjVar(objectId, offset);
                
                // 将字段对象加入目标节点的pts
                if (pts[dstId].insert(fieldObjId).second) {
                    changed = true;
                }
            }
            
            // 如果pts发生变化，将目标节点加入工作列表
            if (changed) {
                worklist.push(dstId);
            }
        }
        
        // 2.5 处理变量Gep边（Variant Gep）
        // 对于偏移量不确定的情况，需要保守处理
        const SVF::ConstraintEdge::ConstraintEdgeSetTy& varGepOutEdges = node->getVarGepOutEdges();
        for (auto edge : varGepOutEdges) {
            SVF::NodeID dstId = edge->getDstID();
            bool changed = false;
            
            // 对于变量偏移，保守地处理
            // 将基对象直接加入（这是一种简化处理）
            for (SVF::NodeID objectId : currentPts) {
                // 尝试添加基对象本身
                if (pts[dstId].insert(objectId).second) {
                    changed = true;
                }
                
                // 也可以尝试添加所有可能的字段
                // 但这需要知道对象的结构信息
            }
            
            // 如果pts发生变化，将目标节点加入工作列表
            if (changed) {
                worklist.push(dstId);
            }
        }
    }
}
