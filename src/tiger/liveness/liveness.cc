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

void MoveList::UnionWith(MoveList *list) {
  for (auto move : list->GetList()) {
    if (!Contain(move.first, move.second))
      move_list_.push_back(move);
  }
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
  auto allN = flowgraph_->Nodes()->GetList();
  while (!done) {
    done = true;
    /* loop backwards => reach fixpoint faster */
    for (auto curNode = allN.rbegin(); curNode != allN.rend(); ++curNode) {
      /* tons of memory leak here, but I dont give a shit */
      auto inSet = in_->Look(*curNode);
      auto outSet = out_->Look(*curNode);
      auto instr = (*curNode)->NodeInfo();
      auto newInSet = instr->Use()->Union(outSet->Diff(instr->Def()));
      auto newOutSet = new temp::TempList();
      for (auto succNode : (*curNode)->Succ()->GetList()) {
        newOutSet->UnionWith(in_->Look(succNode));
      }
      in_->Set(*curNode, newInSet);
      out_->Set(*curNode, newOutSet);
      if (!newInSet->IsEquivalent(inSet) || !newOutSet->IsEquivalent(outSet))
        done = false;
      delete inSet, delete outSet;
    }
  }
}

void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  
  auto interf_graph_ = live_graph_.interf_graph;
  auto move_list_ = live_graph_.moves;
  /* phase1: insert all pre-colored regs */
  for (auto reg : reg_manager->Registers()->GetList()) {
    /* rsp not allowed here */
    INode *node = interf_graph_->NewNode(reg);
    temp_node_map_->Enter(reg, node);
  }
  for (auto from : reg_manager->ArgRegs()->GetList()) {
    for (auto to : reg_manager->ArgRegs()->GetList()) {
      if (from == to) continue;
      interf_graph_->AddEdge(AskNode(from), AskNode(to));
    }
  }
  /* phase2: add temps to flowgraph */
  for (auto fNode : flowgraph_->Nodes()->GetList()) {
    auto instr = fNode->NodeInfo();
    /* code can be optmized here */
    if (fNode->NodeInfo()->IsMove()) {
      for (auto defTemp : instr->Def()->GetList()) {
        auto defNode = AskNode(defTemp);
        /* add interference edge: not in use-list */
        for (auto outTemp : out_->Look(fNode)->Diff(instr->Use())->GetList()) {
          if (defTemp == outTemp) continue;
          auto outNode = AskNode(outTemp);
          interf_graph_->AddEdge(defNode, outNode);
          interf_graph_->AddEdge(outNode, defNode);
        }
        /* construct move list */
        for (auto useTemp : instr->Use()->GetList()) {
          move_list_->Append(AskNode(useTemp), defNode);
        }
      }
    } else {
      for (auto defTemp : instr->Def()->GetList()) {
        auto defNode = AskNode(defTemp);
        for (auto outTemp : out_->Look(fNode)->GetList()) {
          if (defTemp == outTemp) continue;
          auto outNode = AskNode(outTemp);
          interf_graph_->AddEdge(defNode, outNode);
          interf_graph_->AddEdge(outNode, defNode);
        }
      }
    }
  }
}

INodePtr LiveGraphFactory::AskNode(temp::Temp *reg) {
  INodePtr node = temp_node_map_->Look(reg);
  if (node == nullptr) {
    node = live_graph_.interf_graph->NewNode(reg);
    temp_node_map_->Enter(reg, node);
  }
  return node;
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
  // PrintInAndOut(reg_manager);
}

void PrintTempList(temp::TempList *tl, frame::RegManager *rm) {
  for (auto temp : tl->GetList()) {
    auto res = rm->temp_map_->Look(temp);
    if (res) {
      printf("%s ", res->c_str());
    } else {
      printf("t%d ", temp->Int());
    }
  }
  printf("\n");
}

void LiveGraphFactory::PrintInAndOut(frame::RegManager *rm) {
  for (auto fnode : flowgraph_->Nodes()->GetList()) {
    printf("In : ");
    PrintTempList(in_->Look(fnode), rm);
    printf("Out: ");
    PrintTempList(out_->Look(fnode), rm);
  }
}

} // namespace live