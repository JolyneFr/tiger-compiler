#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;


} // namespace

namespace cg {

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  auto list = new assem::InstrList();
  for (auto stm :traces_->GetStmList()->GetList())
    stm->Munch(*list, fs_);

  assem_instr_ = 
    std::make_unique<AssemInstr>(frame::ProcEntryExit2(list));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */

std::string fsPlaceHolder(std::string_view fs) {
  return std::string(fs) + "_framesize";
}

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::LabelInstr(
    temp::LabelFactory::LabelString(label_), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  assem::Instr *jmpInstr = new assem::OperInstr(
    "jmp `j0", 
    nullptr, nullptr, new assem::Targets(jumps_));

  instr_list.Append(jmpInstr);
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *leftTemp = left_->Munch(instr_list, fs);
  temp::Temp *rightTemp = right_->Munch(instr_list, fs);

  assem::Instr *cmpInstr = new assem::OperInstr(
    "cmpq `s0, `s1", 
    nullptr, new temp::TempList({rightTemp, leftTemp}), nullptr);

  std::string cjumpAssem;
  switch (op_) {
    case EQ_OP: cjumpAssem = "je `j0"; break;
    case NE_OP: cjumpAssem = "jne `j0"; break;
    case LT_OP: cjumpAssem = "jl `j0"; break;
    case LE_OP: cjumpAssem = "jle `j0"; break;
    case GT_OP: cjumpAssem = "jg `j0"; break;
    case GE_OP: cjumpAssem = "jge `j0"; break;
    default: assert(0);
  }
  assem::Instr *cjumpInstr = new assem::OperInstr(
    /* Example: "jle `j0" or "jg `j0"... */
    cjumpAssem, nullptr, nullptr, 
    new assem::Targets(new std::vector<temp::Label*>({true_label_})));

  instr_list.Append(cmpInstr);
  instr_list.Append(cjumpInstr);
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /* these cases are insane */
  if (typeid(*dst_) == typeid(tree::MemExp)) {
    tree::MemExp *memDst = static_cast<tree::MemExp*>(dst_);

    if (typeid(*memDst->exp_) == typeid(tree::BinopExp) && 
        static_cast<tree::BinopExp*>(memDst->exp_)->op_ == tree::PLUS_OP) {
      auto binopExp = static_cast<tree::BinopExp*>(memDst->exp_);
      temp::Temp *t1, *t2;  int consti;

      if (typeid(*binopExp->left_) == typeid(tree::ConstExp)) {
        /* MOVE(MEM(BINOP(PLUS, CONST(i), e1)), e2) */
        consti = static_cast<tree::ConstExp*>(binopExp->left_)->consti_;
        t1 = binopExp->right_->Munch(instr_list, fs);
        t2 = src_->Munch(instr_list, fs);
      } else 

      if (typeid(*binopExp->right_) == typeid(tree::ConstExp)) {
        /* MOVE(MEM(BINOP(PLUS, CONST(i), e1)), e2) */
        consti = static_cast<tree::ConstExp*>(binopExp->right_)->consti_;
        t1 = binopExp->left_->Munch(instr_list, fs);
        t2 = src_->Munch(instr_list, fs);
      } else 

      /* fall through to normal condition: MOVE(MEM(e1), e2) */ {
        t1 = memDst->exp_->Munch(instr_list, fs);
        t2 = src_->Munch(instr_list, fs);
        assem::Instr *store = new assem::MoveInstr(
          "movq `s0, (`s1)",nullptr, new temp::TempList({t2, t1}));
        instr_list.Append(store); return;
      }

      assem::Instr *binMemDstMove = new assem::MoveInstr(
        "movq `s0, " + std::to_string(consti) + "(`s1)",
        nullptr, new temp::TempList({t2, t1}));
      instr_list.Append(binMemDstMove);
    } else 

    if (typeid(*src_) == typeid(tree::MemExp)) {
      /* MOVE(MEM(e1), MEM(e2)) */
      tree::MemExp *memSrc = static_cast<tree::MemExp*>(src_);
      temp::Temp *t = temp::TempFactory::NewTemp();
      temp::Temp *t1 = memDst->exp_->Munch(instr_list, fs);
      temp::Temp *t2 = memSrc->exp_->Munch(instr_list, fs);
      assem::Instr *loadFrom = new assem::MoveInstr(
        "movq (`s0), `d0", new temp::TempList(t), new temp::TempList(t2));
      assem::Instr *storeTo = new assem::MoveInstr(
        "movq `s0, (`s1)", nullptr, new temp::TempList({t, t1}));
      instr_list.Append(loadFrom); 
      instr_list.Append(storeTo);
    } else 
    
    if (typeid(*src_) == typeid(tree::ConstExp)) {
      /* MOVE(MEM(e1), CONST(i)) */
      int consti = static_cast<tree::ConstExp*>(src_)->consti_;
      temp::Temp *t1 = memDst->exp_->Munch(instr_list, fs);
      assem::Instr *storeImm = new assem::MoveInstr(
        "movq $" + std::to_string(consti) + ", (`s0)",
        nullptr, new temp::TempList(t1));
      instr_list.Append(storeImm);
    } else 
    
    /* all other conditions */ {
      /* MOVE(MEM(e1), e2) */
      temp::Temp *t1 = memDst->exp_->Munch(instr_list, fs);
      temp::Temp *t2 = src_->Munch(instr_list, fs);
      assem::Instr *store = new assem::MoveInstr(
        "movq `s0, (`s1)",nullptr, new temp::TempList({t2, t1}));
      instr_list.Append(store);
    }
  } else {
    /* dst must be temp, and can't be %rbp */
    assert(typeid(*dst_) == typeid(tree::TempExp));
    assert(static_cast<tree::TempExp*>(dst_)->temp_ != reg_manager->FramePointer());
    temp::Temp *dstTemp = dst_->Munch(instr_list, fs);
    temp::Temp *srcTemp = src_->Munch(instr_list, fs);
    assem::Instr *move = new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(dstTemp), new temp::TempList(srcTemp));
    instr_list.Append(move);
  }

}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *lt = left_->Munch(instr_list, fs);
  temp::Temp *rt = right_->Munch(instr_list, fs);
  temp::Temp *res = temp::TempFactory::NewTemp();

  if (op_ == tree::PLUS_OP || op_ == tree::MINUS_OP) {
    std::string binopAssem;
    switch (op_) {
      case tree::PLUS_OP: binopAssem = "addq `s0, `d0"; break;
      case tree::MINUS_OP: binopAssem = "subq `s0, `d0"; break;
    }
    /* move e1 to res first: no side-effect */
    assem::Instr *move = new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(res), new temp::TempList(lt));
    assem::Instr *op = new assem::OperInstr(
      binopAssem,
      new temp::TempList(res), new temp::TempList({rt, res}), nullptr);
    instr_list.Append(move);
    instr_list.Append(op);
  } else if (op_ == tree::MUL_OP) {
    assem::Instr *move1 = new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(frame::X64RegManager::rax), new temp::TempList(lt));
    assem::Instr *imul = new assem::OperInstr(
      "imulq `s0",
      new temp::TempList(frame::X64RegManager::rax), new temp::TempList(rt), nullptr);
    assem::Instr *move2 = new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(res), new temp::TempList(frame::X64RegManager::rax));
    instr_list.Append(move1);
    instr_list.Append(imul);
    instr_list.Append(move2);
  } else if (op_ == tree::DIV_OP) {
    assem::Instr *move1 = new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(frame::X64RegManager::rax), new temp::TempList(lt));
    assem::Instr *cqto = new assem::OperInstr(
      "cqto", 
      new temp::TempList({frame::X64RegManager::rax, frame::X64RegManager::rdx}), 
      new temp::TempList(frame::X64RegManager::rax), nullptr);
    assem::Instr *idiv = new assem::OperInstr(
      "idivq `s0",
      new temp::TempList({frame::X64RegManager::rax, frame::X64RegManager::rdx}),
      new temp::TempList({rt, frame::X64RegManager::rax, frame::X64RegManager::rdx}),
      nullptr);
    assem::Instr *move2 = new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(res), new temp::TempList(frame::X64RegManager::rax));
    instr_list.Append(move1);
    instr_list.Append(cqto);
    instr_list.Append(idiv);
    instr_list.Append(move2);
  } else assert(0);

  return res;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *resReg = temp::TempFactory::NewTemp();
  if (typeid(*exp_) == typeid(tree::BinopExp) && 
      static_cast<tree::BinopExp*>(exp_)->op_ == tree::PLUS_OP) {
    auto binopExp = static_cast<tree::BinopExp*>(exp_);
    temp::Temp *t; int consti;
    if (typeid(*binopExp->left_) == typeid(tree::ConstExp)) {
      consti = static_cast<tree::ConstExp*>(binopExp->left_)->consti_;
      t = binopExp->right_->Munch(instr_list, fs);
    } else 

    if (typeid(*binopExp->right_) == typeid(tree::ConstExp)) {
      consti = static_cast<tree::ConstExp*>(binopExp->right_)->consti_;
      t = binopExp->left_->Munch(instr_list, fs);
    } else

    /* noconst, dont use goto statement */ {
      t = exp_->Munch(instr_list, fs);
      assem::Instr *load = new assem::MoveInstr(
        "movq (`s0), `d0",
        new temp::TempList(resReg), new temp::TempList(t));
      instr_list.Append(load); 
      return resReg;
    }

    assem::Instr *offsetLoad = new assem::MoveInstr(
      "movq " + std::to_string(consti) + "(`s0), `d0",
      new temp::TempList(resReg), new temp::TempList(t));
    instr_list.Append(offsetLoad);
  } else {
    temp::Temp *t = exp_->Munch(instr_list, fs);
    assem::Instr *load = new assem::MoveInstr(
      "movq (`s0), `d0",
      new temp::TempList(resReg), new temp::TempList(t));
    instr_list.Append(load);
  }

  return resReg;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  if (temp_ == reg_manager->FramePointer()) {
    temp::Temp *res = temp::TempFactory::NewTemp();
    assem::Instr *leaqInstr = new assem::MoveInstr(
      "leaq " + fsPlaceHolder(fs) + "(`s0), `d0", 
      new temp::TempList(res), new temp::TempList(reg_manager->StackPointer()));

    instr_list.Append(leaqInstr);
    return res;
  }
  return temp_;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *res = temp::TempFactory::NewTemp();
  assem::Instr *name = new assem::OperInstr(
    "leaq " + name_->Name() + "(%rip), `d0",
    new temp::TempList(res), nullptr, nullptr);

  instr_list.Append(name);
  return res;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *res = temp::TempFactory::NewTemp();
  assem::Instr *moveImmInstr = new assem::MoveInstr(
    "movq $" + std::to_string(consti_) + ", `d0",
    new temp::TempList(res), nullptr);

  instr_list.Append(moveImmInstr);
  return res;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::TempList *usedRegs = args_->MunchArgs(instr_list, fs);
  temp::Temp *resReg = temp::TempFactory::NewTemp();
  std::string funcName = temp::LabelFactory::LabelString(static_cast<tree::NameExp*>(fun_)->name_);
  assem::Instr *call = new assem::OperInstr(
    "callq " + funcName,
    reg_manager->CallerSaves(), usedRegs, nullptr);
  assem::Instr *moveRet = new assem::MoveInstr(
    "movq `s0, `d0",
    new temp::TempList(resReg), new temp::TempList(reg_manager->ReturnValue()));

  instr_list.Append(call);
  instr_list.Append(moveRet);
  return resReg;
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto argRegs = reg_manager->ArgRegs()->GetList();
  auto regItr = argRegs.begin();
  temp::TempList *usedRegs = new temp::TempList();
  int offset = frame::WORD_SIZE;
  for (auto exp : exp_list_) {
    temp::Temp *expRes = exp->Munch(instr_list, fs);
    if (regItr != argRegs.end()) {
      assem::Instr *moveToReg = new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList(*regItr), new temp::TempList(expRes));
      instr_list.Append(moveToReg);
      usedRegs->Append(*regItr);
      regItr++; 
    } else {
      assem::Instr *moveToFrame = new assem::MoveInstr(
        "movq `s0, " + std::to_string(offset) + "(`d0)",
        new temp::TempList(reg_manager->StackPointer()), new temp::TempList(expRes));
      instr_list.Append(moveToFrame);
      offset += frame::WORD_SIZE;
    }
  }

  return usedRegs;
}

} // namespace tree
