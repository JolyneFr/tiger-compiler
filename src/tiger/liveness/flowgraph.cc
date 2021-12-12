#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {

  FNodePtr prevNode = nullptr;
  for (auto instr : instr_list_->GetList()) {
    FNodePtr curNode = flowgraph_->NewNode(instr);
    if (prevNode) {
      /* prev is not direct jump: have an edge */
      flowgraph_->AddEdge(prevNode, curNode);
    }
    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      temp::Label *label = static_cast<assem::LabelInstr*>(instr)->label_;
      label_map_->Enter(label, curNode);
    }
    if (instr->IsDirectJmp()) prevNode = nullptr;
    else prevNode = curNode;
  }
  /* add jump edge to control flow */
  for (auto fromNode : flowgraph_->Nodes()->GetList()) {
    if (fromNode->NodeInfo()->IsJmp()) {
      auto targetLabels = *(static_cast<assem::OperInstr*>(fromNode->NodeInfo())->jumps_->labels_);
      for (auto label : targetLabels) {
        FNodePtr toNode = label_map_->Look(label);
        assert(toNode != nullptr);
        flowgraph_->AddEdge(fromNode, toNode);
      }
    }
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  return new temp::TempList();
}

temp::TempList *MoveInstr::Def() const {
  return dst_ ? dst_ : new temp::TempList();
}

temp::TempList *OperInstr::Def() const {
  return dst_ ? dst_ : new temp::TempList();
}

temp::TempList *LabelInstr::Use() const {
  return new temp::TempList();
}

temp::TempList *MoveInstr::Use() const {
  return src_ ? src_ : new temp::TempList();
}

temp::TempList *OperInstr::Use() const {
  return src_ ? src_ : new temp::TempList();
}
} // namespace assem
