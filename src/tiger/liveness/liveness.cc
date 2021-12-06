#include "tiger/liveness/liveness.h"

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (!Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  /* intialize in & out set */
  for (auto node : flowgraph_->Nodes()->GetList()) {
    in_->Enter(node, new temp::TempList());
    out_->Enter(node, new temp::TempList());
  }
  /* calculate in & out till reach fixed-point */
  bool done = false;
  while (!done) {
    done = true;
    for (auto curNode : flowgraph_->Nodes()->GetList()) {
      /* tons of memory leak here, but I dont give a shit */
      auto inSet = in_->Look(curNode);
      auto outSet = out_->Look(curNode);
      auto instr = curNode->NodeInfo();
      auto newInSet = instr->Use()->Union(outSet->Diff(instr->Def()));
      auto newOutSet = new temp::TempList();
      for (auto succNode : curNode->Succ()->GetList()) {
        newOutSet = newOutSet->Union(in_->Look(succNode));
      }
      in_->Set(curNode, newInSet);
      out_->Set(curNode, newOutSet);
      if (!newInSet->IsEquivalent(inSet) || !newOutSet->IsEquivalent(outSet))
        done = false;
    }

  }
}

void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  /* phase1: insert all pre-colored regs */
  auto sp = reg_manager->StackPointer();
  for (auto reg : reg_manager->ArgRegs()->GetList()) {
    /* rsp not allowed here */
    if (reg == sp) continue;
    INode *node = live_graph_.interf_graph->NewNode(reg);
    temp_node_map_->Enter(reg, node);
  }
  for (auto from : reg_manager->ArgRegs()->GetList()) {
    for (auto to : reg_manager->ArgRegs()->GetList()) {
      if (from == to || from == sp || to == sp) continue;
      live_graph_.interf_graph->AddEdge(AskNode(from), AskNode(to));
    }
  }
  /* phase2: add flowgraph */
  for (auto fNode : flowgraph_->Nodes()->GetList()) {
    auto instr = fNode->NodeInfo();
    auto useTempList = instr->Use();
    /* code can be optmized here */
    if (typeid(*fNode->NodeInfo()) == typeid(assem::MoveInstr)) {
      for (auto defTemp : instr->Def()->GetList()) {
        auto defNode = AskNode(defTemp);
        for (auto outTemp : out_->Look(fNode)->GetList()) {
          if (useTempList->Contain(outTemp)) continue;
          auto outNode = AskNode(outTemp);
          live_graph_.interf_graph->AddEdge(defNode, outNode);
          live_graph_.moves->Append(defNode, outNode);
        }
      }
    } else if (typeid(*fNode->NodeInfo()) == typeid(assem::OperInstr)) {
      for (auto defTemp : instr->Def()->GetList()) {
        auto defNode = AskNode(defTemp);
        for (auto outTemp : out_->Look(fNode)->GetList()) {
          live_graph_.interf_graph->AddEdge(defNode, AskNode(outTemp));
        }
      }
    }
  }
}

INodePtr LiveGraphFactory::AskNode(temp::Temp *reg) {
  INodePtr node = temp_node_map_->Look(reg);
  if (!node) {
    INodePtr newNode = live_graph_.interf_graph->NewNode(reg);
    temp_node_map_->Enter(reg, newNode);
  } else return node;
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live