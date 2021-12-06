#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {

RegAllocator::RegAllocator(frame::Frame *frame_, std::unique_ptr<cg::AssemInstr> assem_instr):
  frame_(frame_), assem_instr_(std::move(assem_instr)), 
  simplifyWorkList(), freezeWorkList(), spillWorkList(), spilledNodes(), workListMoves() {

}

std::unique_ptr<Result> RegAllocator::TransferResult() {
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
  workListMoves = liveGraph.moves;
}

void RegAllocator::Build() {
  
}


} // namespace ra