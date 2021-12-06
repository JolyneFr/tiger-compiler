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

namespace ra {

using Node = graph::Node<temp::Temp>;
using NodeListPtr = graph::NodeList<temp::Temp>*;
using MoveListPtr = live::MoveList*;
using LiveGraph = live::LiveGraph;

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
  ~Result();
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

  void AddEdge(Node *from, Node *to);

  /* data members */
  NodeListPtr simplifyWorkList;
  NodeListPtr freezeWorkList;
  NodeListPtr spillWorkList;

  NodeListPtr spilledNodes;
  NodeListPtr coloredNodes;
  NodeListPtr coalescedNodes;

  MoveListPtr workListMoves;

  LiveGraph liveGraph;

  
  std::unique_ptr<Result> result_;
  frame::Frame *frame_;
  std::unique_ptr<cg::AssemInstr> assem_instr_;


};

} // namespace ra

#endif