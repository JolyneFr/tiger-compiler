#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

#include <set>
#include <stack>
#include <map>

namespace ra {

using Node = graph::Node<temp::Temp>;
using NodePtr = graph::Node<temp::Temp>*;
using NodeList = graph::NodeList<temp::Temp>;
using NodeListPtr = graph::NodeList<temp::Temp>*;
using MoveList = live::MoveList;
using MoveListPtr = live::MoveList*;
using LiveGraph = live::LiveGraph;

using NodeMoveMap = std::map<Node*, MoveList*>;
using DegreeMap = std::map<NodePtr, int>;
using AliasMap = std::map<NodePtr, NodePtr>;
using ColorMap = std::map<NodePtr, temp::Temp*>;

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result() = default;
};

class RegAllocator {
public:
  RegAllocator(frame::Frame *frame_, std::unique_ptr<cg::AssemInstr> assem_instr);
  std::unique_ptr<Result> TransferResult();
  void RegAlloc();

private:
  /* private used functions */
  void LivenessAnalysis();
  void Build();
  void MakeWorkList();
  void Simplify();
  void Coalesce();
  void Freeze();
  void SelectSpill();
  void AssignColor();
  void RewriteProgram();

  void AddEdge(Node *u, Node *v);
  bool MoveRelated(Node *n);
  MoveListPtr NodeMoves(Node *n);
  bool PreColored(Node *n);
  NodeListPtr Adjacent(Node *n);
  void DecrementDegree(Node *n);
  void EnableMoves(NodeList *nl);
  void ClearAll();
  NodePtr GetAlias(Node *n);
  void AddWorkList(Node *n);
  bool OK(Node *u, Node *v);
  bool Conservative(NodeList *nl);
  void Combine(Node *u, Node *v);
  void FreezeMoves(Node *u);

  /* Node set: except precolored & inital */
  NodeListPtr simplifyWorkList;
  NodeListPtr freezeWorkList;
  NodeListPtr spillWorkList;
  NodeListPtr spilledNodes;
  NodeListPtr coloredNodes;
  NodeListPtr coalescedNodes;
  NodeListPtr selectStack;

  MoveListPtr workListMoves;
  MoveListPtr activeMoves;
  MoveListPtr frozenMoves;
  MoveListPtr constrainedMoves;
  MoveListPtr coalescedMoves;

  NodeMoveMap moveList;
  AliasMap alias;
  DegreeMap degree;
  ColorMap color;

  LiveGraph liveGraph;


  
  std::unique_ptr<Result> result_;
  frame::Frame *frame_;
  std::unique_ptr<cg::AssemInstr> assem_instr_;
  int K;
  


};

} // namespace ra

#endif