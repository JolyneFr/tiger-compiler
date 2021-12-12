#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {

RegAllocator::RegAllocator(frame::Frame *frame_, std::unique_ptr<cg::AssemInstr> assem_instr):
  frame_(frame_), assem_instr_(std::move(assem_instr)), 
  simplifyWorkList(new NodeList()), freezeWorkList(new NodeList()), spillWorkList(new NodeList()), 
  spilledNodes(new NodeList()), coloredNodes(new NodeList()), coalescedNodes(new NodeList()),
  selectStack(new NodeList()), workListMoves(new MoveList()), activeMoves(new MoveList()),
  frozenMoves(new MoveList()), constrainedMoves(new MoveList()), coalescedMoves(new MoveList()),
  liveGraph(nullptr, nullptr), K(static_cast<int>(reg_manager->Registers()->GetList().size())),
  result_(std::make_unique<Result>(temp::Map::Empty(), nullptr)) {}

std::unique_ptr<Result> RegAllocator::TransferResult() {
  /* construct color-map */
  auto coloring = result_->coloring_;
  auto temp_map = reg_manager->temp_map_;
  for (const auto &tempColor : color) {
    coloring->Enter(tempColor.first->NodeInfo(), temp_map->Look(tempColor.second));
    if (PreColored(tempColor.first)) {
      assert(tempColor.first->NodeInfo() == tempColor.second);
    }
  }
  TigerLog("list size is %ld\n", assem_instr_->GetInstrList()->GetList().size());
  result_->il_ = assem_instr_->GetInstrList()->Compressed(coloring);
  return std::move(result_);
}

void RegAllocator::RegAlloc() {
    LivenessAnalysis();
    Build();
    MakeWorkList();
    while (!simplifyWorkList->Empty() || !workListMoves->Empty() || 
        !freezeWorkList->Empty() || !spillWorkList->Empty()) {
      if (!simplifyWorkList->Empty()) Simplify();
      else if (!workListMoves->Empty()) Coalesce();
      else if (!freezeWorkList->Empty()) Freeze();
      else if (!spillWorkList->Empty()) SelectSpill();
    }
    AssignColor();
    if (!spilledNodes->Empty()) {
      TigerLog("%ld nodes spilled!\n", spilledNodes->GetList().size());
      RewriteProgram();
      RegAlloc();
    }
}

void RegAllocator::LivenessAnalysis() {
  fg::FlowGraphFactory fgFactory(assem_instr_->GetInstrList());
  fgFactory.AssemFlowGraph();
  live::LiveGraphFactory lgFactory(fgFactory.GetFlowGraph());
  lgFactory.Liveness();
  liveGraph = lgFactory.GetLiveGraph();
}

void RegAllocator::Build() {
  ClearAll();
  /* add all moves to workListMoves */
  workListMoves = liveGraph.moves;
  /* constrcut move_list */
  for (auto node : liveGraph.interf_graph->Nodes()->GetList()) {
    auto temp = node->NodeInfo();
    
    if (PreColored(node)) {
      color[node] = temp;
      degree[node] = std::numeric_limits<int>::max();
    } else {
      /* OutDegree == InDegree */
      degree[node] = node->OutDegree();
    }
      
    /* construct move_list */
    MoveListPtr moves = new MoveList();
    for (const auto &move : workListMoves->GetList()) {
      if (move.first == node || move.second == node) {
        moves->Append(move.first, move.second);
      }
    }
    moveList[node] = moves;
  }
}

void RegAllocator::MakeWorkList() {
  for (auto node : liveGraph.interf_graph->Nodes()->GetList()) {
    if (PreColored(node)) continue;
    if (node->OutDegree() >= K) 
      spillWorkList->Append(node);
    else if (MoveRelated(node))
      freezeWorkList->Append(node);
    else 
      simplifyWorkList->Append(node);
  }
}

void RegAllocator::Simplify() {
  NodePtr node = simplifyWorkList->GetOne();
  simplifyWorkList->DeleteNode(node);
  selectStack->Push(node);
  for (auto adjNode : Adjacent(node)->GetList()) {
    DecrementDegree(adjNode);
  }
}

void RegAllocator::Coalesce() {
  /* (frist, second) = (x, y) */
  auto move = workListMoves->GetOne();
  Node *u, *v;
  if (PreColored(move.second)) {
    v = GetAlias(move.first);
    u = GetAlias(move.second);
  } else {
    u = GetAlias(move.first);
    v = GetAlias(move.second);
  }
  workListMoves->Delete(move.first, move.second);
  if (u == v) {
    coalescedMoves->Append(move.first, move.second);
    AddWorkList(u);
  } else if (PreColored(v) || u->Adj()->Contain(v)) {
    constrainedMoves->Append(move.first, move.second);
    AddWorkList(u);
    AddWorkList(v);
  } else if ((PreColored(u) && OK(u, v)) || 
    (!PreColored(u) && Conservative(Adjacent(u)->Union(Adjacent(v))))) {
    coalescedMoves->Append(move.first, move.second);
    Combine(u, v);
    AddWorkList(u);
  } else {
    activeMoves->Append(move.first, move.second);
  }
  
}

void RegAllocator::Freeze() {
  auto u = freezeWorkList->GetOne();
  freezeWorkList->DeleteNode(u);
  simplifyWorkList->Append(u);
  FreezeMoves(u);
}

void RegAllocator::SelectSpill() {
  /* should be heuristic */
  auto m = spillWorkList->GetOne();
  spillWorkList->DeleteNode(m);
  simplifyWorkList->Append(m);
  FreezeMoves(m);
}

void RegAllocator::AssignColor() {
  while (!selectStack->Empty()) {
    auto n = selectStack->Pop();
    assert(!PreColored(n));
    auto okColors = new temp::TempList(reg_manager->Registers());
    for (auto w : n->Adj()->GetList()) {
      auto aliasW = GetAlias(w);
      if (coloredNodes->Contain(aliasW) || PreColored(aliasW)) {
        okColors->Delete(color[aliasW]);
        // TigerLog("delete color %s for t%d\n", reg_manager->temp_map_->Look(color[aliasW])->c_str(), n->NodeInfo()->Int());
      }
    }
    if (okColors->Empty()) {
      spilledNodes->Append(n);
    } else {
      coloredNodes->Append(n);
      color[n] = okColors->GetOne();
      TigerLog("assign color %s for t%d\n", reg_manager->temp_map_->Look(color[n])->c_str(), n->NodeInfo()->Int());
    }
  }
  for (auto n : coalescedNodes->GetList()) {
    color[n] = color[GetAlias(n)];
  }
}

void RegAllocator::RewriteProgram() {
  
  for (auto node : spilledNodes->GetList()) {
    TigerLog("spill temp: t%d\n", node->NodeInfo()->Int());
    auto spilledTemp = node->NodeInfo();
    /* must alloc space in frame */
    frame_->AllocLocal(true);
    int offset = frame_->Size();
    auto instrList = assem_instr_->GetInstrList();
    std::string fs = tree::fsPlaceHolder(frame_->Name()->Name());
    auto sp = reg_manager->StackPointer();
    auto instrItr = instrList->GetList().cbegin();
    while (instrItr != instrList->GetList().cend()) {
      auto instr = *instrItr;
      temp::TempList *src = nullptr;
      temp::TempList *dst = nullptr;
      if (typeid(*instr) == typeid(assem::OperInstr)) {
        src = static_cast<assem::OperInstr*>(instr)->src_;
        dst = static_cast<assem::OperInstr*>(instr)->dst_;
      } else if (typeid(*instr) == typeid(assem::MoveInstr)) {
        src = static_cast<assem::MoveInstr*>(instr)->src_;
        dst = static_cast<assem::MoveInstr*>(instr)->dst_;
      }

      if ((src && src->Contain(spilledTemp)) || (dst && dst->Contain(spilledTemp))) {
        /* create new temp vi for each use / def of spilled temp v */
        temp::Temp *vi = temp::TempFactory::NewTemp();
        if (src && src->Contain(spilledTemp)) {
          /* use spilled temp: insert load-instr before it */
          char load[77];
          sprintf(load, "movq (%s - %d)(`s0), `d0", fs.c_str(), offset);
          assem::Instr *loadFromFrame = new assem::MoveInstr(
            load, new temp::TempList(vi), new temp::TempList(sp));
          assem_instr_->GetInstrList()->Insert(instrItr, loadFromFrame);
          src->Replace(spilledTemp, vi);
        }

        if (dst && dst->Contain(spilledTemp)) {
          /* def spilled temp: insert store-instr after it */
          char store[77];
          sprintf(store, "movq `s0, (%s - %d)(`s1)", fs.c_str(), offset);
          assem::Instr *storeToFrame = new assem::MoveInstr(
            store, nullptr, new temp::TempList({vi, sp}));
          assem_instr_->GetInstrList()->Insert(std::next(instrItr), storeToFrame);
          dst->Replace(spilledTemp, vi);
          ++instrItr;
        }
      } 
      ++instrItr;
    }
  }
  spilledNodes->Clear();
  coloredNodes->Clear();
  coalescedNodes->Clear();
}

void RegAllocator::AddEdge(Node *u, Node *v) {
  if (u->Adj()->Contain(v) || u == v) return;
  if (!PreColored(u)) {
    liveGraph.interf_graph->AddEdge(u, v);
    degree[u]++;
  } 
  if (!PreColored(v)) {
    liveGraph.interf_graph->AddEdge(v, u);
    degree[v]++;
  } 
}

bool RegAllocator::MoveRelated(Node *n) {
  return !NodeMoves(n)->GetList().empty();
}

MoveListPtr RegAllocator::NodeMoves(Node *n) {
  return moveList[n]->Intersect(activeMoves->Union(workListMoves));
}

bool RegAllocator::PreColored(Node *n) {
  return reg_manager->Registers()->Contain(n->NodeInfo());
}

NodeListPtr RegAllocator::Adjacent(Node *n) {
  /* undirect graph: Succ is enough */
  return n->Adj()->Diff(selectStack->Union(coalescedNodes));
}

void RegAllocator::DecrementDegree(Node *n) {
  int d = degree[n]--;
  if (d == K) {
    NodeListPtr nl = new NodeList(); nl->Append(n);
    EnableMoves(Adjacent(n)->Union(nl));
    delete nl;
    spillWorkList->DeleteNode(n);
    if (MoveRelated(n)) {
      freezeWorkList->Append(n);
    } else {
      simplifyWorkList->Append(n);
    }
  }
}

void RegAllocator::EnableMoves(NodeList *nl) {
  for (auto node : nl->GetList()) {
    for (auto move : NodeMoves(node)->GetList()) {
      if (activeMoves->Contain(move.first, move.second)) {
        activeMoves->Delete(move.first, move.second);
        workListMoves->Append(move.first, move.second);
      }
    }
  }
}

void RegAllocator::ClearAll() {
  /* clear work_list */
  simplifyWorkList->Clear();
  freezeWorkList->Clear();
  spillWorkList->Clear();
  spilledNodes->Clear();
  coalescedNodes->Clear();
  coloredNodes->Clear();
  selectStack->Clear();
  /* clear other */
  moveList.clear();
  degree.clear();
  alias.clear();
}

NodePtr RegAllocator::GetAlias(Node *n) {
  if (coalescedNodes->Contain(n)) 
    return GetAlias(alias[n]);
  return n;
}

void RegAllocator::AddWorkList(Node *n) {
  if (!PreColored(n) && !MoveRelated(n) && degree[n] < K) {
    freezeWorkList->DeleteNode(n);
    simplifyWorkList->Append(n);
  }
}

/* ok with george's rule */
bool RegAllocator::OK(Node *u, Node *v) {
  for (auto t : Adjacent(v)->GetList()) {
    if (!(degree[t] < K || PreColored(t) || t->Adj()->Contain(u)))
      return false;
  }
  return true;
}

bool RegAllocator::Conservative(NodeList *nodes) {
  int k = 0;
  for (auto n : nodes->GetList()) {
    if (degree[n] >= K) k++;
  }
  return k < K;
}

void RegAllocator::Combine(Node *u, Node *v) {
  if (freezeWorkList->Contain(v)) {
    freezeWorkList->DeleteNode(v);
  } else {
    spillWorkList->DeleteNode(v);
  }
  coalescedNodes->Append(v);
  alias[v] = u;
  moveList[u]->UnionWith(moveList[v]);
  NodeListPtr vnl = new NodeList(); vnl->Append(v);
  EnableMoves(vnl);
  delete vnl;
  for (auto t : Adjacent(v)->GetList()) {
    AddEdge(t, u);
    DecrementDegree(t);
  }
  if (degree[u] >= K && freezeWorkList->Contain(u)) {
    freezeWorkList->DeleteNode(u);
    simplifyWorkList->Append(u);
  }
}

void RegAllocator::FreezeMoves(Node *u) {
  for (auto move : NodeMoves(u)->GetList()) {
    NodePtr v;
    if (GetAlias(move.second) == GetAlias(u)) {
      v = GetAlias(move.first);
    } else {
      v = GetAlias(move.second);
    }
    activeMoves->Delete(move.first, move.second);
    frozenMoves->Append(move.first, move.second);
    if (NodeMoves(v)->Empty() && degree[v] < K) {
      freezeWorkList->DeleteNode(v);
      simplifyWorkList->Append(v);
    }
  }
}

} // namespace ra