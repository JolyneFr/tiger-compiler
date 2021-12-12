#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  return new Access(level, level->frame_->AllocLocal(escape));
}

Level *Level::NewLevel(Level *parent, temp::Label *name, std::list<bool> formals) {
  frame::Frame *newFrame = frame::NewFrame(name, formals);
  return new Level(newFrame, parent);
}

class Cx {
public:
  temp::Label **trues_;
  temp::Label **falses_;
  tree::Stm *stm_;

  Cx(temp::Label **trues, temp::Label **falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() const = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() const = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) const = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() const override { 
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    tree::CjumpStm *cjumpStm = new tree::CjumpStm(tree::NE_OP,
      exp_, new tree::ConstExp(0), nullptr, nullptr);
    return Cx(&(cjumpStm->true_label_), &(cjumpStm->false_label_), cjumpStm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() const override { 
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    assert(0);
    /* can't reach here */
    return Cx(nullptr, nullptr, nullptr);
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(temp::Label** trues, temp::Label** falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}
  
  [[nodiscard]] tree::Exp *UnEx() const override {
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    /* do patch: is this right? */
    *cx_.trues_ = t; *cx_.falses_ = f;
    /* more good-looking format */
    return 
      new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
      new tree::EseqExp(cx_.stm_,
      new tree::EseqExp(new tree::LabelStm(f),
      new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(0)),
      new tree::EseqExp(new tree::LabelStm(t), 
      new tree::TempExp(r))))));
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    return new tree::ExpStm(UnEx());
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override { 
    return cx_;
  }
};

void ProgTr::Translate() {
  FillBaseTEnv();
  FillBaseVEnv();
  tr::ExpAndTy *ret = absyn_tree_->Translate(
    venv_.get(), tenv_.get(), main_level_.get(), nullptr, errormsg_.get());
  frags->PushBack(new frame::ProcFrag(ret->exp_->UnNx(), main_level_.get()->frame_));
}

} // namespace tr

namespace absyn {

tree::Exp *AccessToExp(tr::Access *access, frame::RegManager *rm) {
  return access->access_->ToExp(new tree::TempExp(rm->FramePointer()));
}

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  env::VarEntry *entry = static_cast<env::VarEntry*>(venv->Look(sym_));
  tree::Exp *framePtr = new tree::TempExp(reg_manager->FramePointer());
  tr::Level *curLevel = level, *targetLevel = entry->access_->level_;
  while (curLevel != targetLevel) {
    /* walk up through static link */
    framePtr = new tree::MemExp(
      new tree::BinopExp(
        tree::PLUS_OP, framePtr, 
        /* static link as the first parameter */
        new tree::ConstExp(-frame::WORD_SIZE)));
    curLevel = curLevel->parent_;
  }
  framePtr = entry->access_->access_->ToExp(framePtr);
  return new tr::ExpAndTy(new tr::ExExp(framePtr), entry->ty_);
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *varRes = var_->Translate(venv, tenv, level, label, errormsg);
  type::RecordTy *ty = static_cast<type::RecordTy*>(varRes->ty_->ActualTy());
  auto fieldItr = ty->fields_->GetList().cbegin();
  auto fieldEnd = ty->fields_->GetList().cend();
  uint32_t byteOffset = 0;
  while (fieldItr != fieldEnd) {
    if ((*fieldItr)->name_->Name() == sym_->Name()) {
      tree::Exp *resExp = new tree::MemExp(new tree::BinopExp(
        tree::PLUS_OP, varRes->exp_->UnEx(), new tree::ConstExp(byteOffset)));
      return new tr::ExpAndTy(new tr::ExExp(resExp), (*fieldItr)->ty_);
    }
    /* size of every field is a WordSize */
    byteOffset += frame::WORD_SIZE; fieldItr++;
  }
  assert(0);
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *varRes = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *subRes = subscript_->Translate(venv, tenv, level, label, errormsg);
  type::ArrayTy *arrTy = static_cast<type::ArrayTy*>(varRes->ty_->ActualTy());
  /* calculate byte offset of index */
  tree::Exp *offsetExp = new tree::BinopExp(
    tree::MUL_OP, subRes->exp_->UnEx(), new tree::ConstExp(frame::WORD_SIZE));
  /* ArrayTy appears like pointer */
  tr::Exp *resExp = new tr::ExExp(
    new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, varRes->exp_->UnEx(), offsetExp))
  );
  return new tr::ExpAndTy(resExp, arrTy->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(
    new tr::ExExp(new tree::ConstExp(0)), type::NilTy::Instance()
  );
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(
    new tr::ExExp(new tree::ConstExp(val_)), type::IntTy::Instance()
  );
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  temp::Label *strLabel = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(strLabel, str_));
  return new tr::ExpAndTy(
    new tr::ExExp(new tree::NameExp(strLabel)), type::StringTy::Instance()
  );
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  env::FunEntry *funcEntry = static_cast<env::FunEntry*>(venv->Look(func_));
  /* translate arguments */
  tree::ExpList *argList = new tree::ExpList();
  for (auto absynArg : args_->GetList()) {
    argList->Append(
      /* no need to check type here */
      absynArg->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx()
    );
  }

  if (funcEntry->level_->parent_ != nullptr) {
    /* self defined function: find static link */
    tree::Exp *staticLink = new tree::TempExp(reg_manager->FramePointer());
    tr::Level *funcParentLevel = level;
    while (funcParentLevel != nullptr && funcParentLevel != funcEntry->level_->parent_) {
      staticLink = new tree::MemExp(
        new tree::BinopExp(tree::PLUS_OP,
          /* static link is the first  */
          staticLink, new tree::ConstExp(-frame::WORD_SIZE)));
      funcParentLevel = funcParentLevel->parent_;
    }
    /* pass static link as the first argument of callexp */
    argList->Insert(staticLink);
  }

  return new tr::ExpAndTy(
    new tr::ExExp(new tree::CallExp(new tree::NameExp(func_), argList)),
    funcEntry->result_
  );
  
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *leftExpAndTy = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *rightExpAndTy = right_->Translate(venv, tenv, level, label, errormsg);
  tree::BinOp treeOp;
  bool isLogical = false;
  /* no need to check type, ohh yeah! */
  switch (oper_) {
    case absyn::PLUS_OP: treeOp = tree::PLUS_OP; break;
    case absyn::MINUS_OP: treeOp = tree::MINUS_OP; break;
    case absyn::TIMES_OP: treeOp = tree::MUL_OP; break;
    case absyn::DIVIDE_OP: treeOp = tree::DIV_OP; break;
    default: isLogical = true;
  }

  if (!isLogical)
    return new tr::ExpAndTy(
      new tr::ExExp(new tree::BinopExp(treeOp, 
        leftExpAndTy->exp_->UnEx(), rightExpAndTy->exp_->UnEx())), 
      type::IntTy::Instance());

  /* logical operator: translate to cjump */
  tree::RelOp relOp;
  switch (oper_) {
    case absyn::EQ_OP: relOp = tree::EQ_OP; break;
    case absyn::NEQ_OP: relOp = tree::NE_OP; break;
    case absyn::LT_OP: relOp = tree::LT_OP; break;
    case absyn::LE_OP: relOp = tree::LE_OP; break;
    case absyn::GT_OP: relOp = tree::GT_OP; break;
    case absyn::GE_OP: relOp = tree::GE_OP; break;
    default: assert(0);
  }

  temp::Label *trueLabel = temp::LabelFactory::NewLabel();
  temp::Label *falseLabel = temp::LabelFactory::NewLabel();
  tree::CjumpStm *cjumpStm;
  /* str1 EQ str2: call external function */
  if (relOp == tree::EQ_OP && 
    leftExpAndTy->ty_->ActualTy() == type::StringTy::Instance() && 
    rightExpAndTy->ty_->ActualTy() == type::StringTy::Instance()) {
    tree::Exp *callStringEqualExp = frame::ExternalCall(
      "string_equal",
      new tree::ExpList({ leftExpAndTy->exp_->UnEx(), rightExpAndTy->exp_->UnEx() }));
    cjumpStm = new tree::CjumpStm(tree::EQ_OP,
      callStringEqualExp, new tree::ConstExp(1), trueLabel, falseLabel);
  } else {
    cjumpStm = new tree::CjumpStm(relOp, 
      leftExpAndTy->exp_->UnEx(), rightExpAndTy->exp_->UnEx(), trueLabel, falseLabel);
  }
  /* maybe no problem */
  return new tr::ExpAndTy(
    new tr::CxExp(&(cjumpStm->true_label_), &(cjumpStm->false_label_), cjumpStm),
    type::IntTy::Instance());
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,      
                                   err::ErrorMsg *errormsg) const {
  type::RecordTy *recordTy = static_cast<type::RecordTy*>(tenv->Look(typ_));
  /* allocate heap space for record */
  int recordByteSize = frame::WORD_SIZE * fields_->GetList().size();
  tree::Exp *callAllocExp = frame::ExternalCall(
    "alloc_record",
    new tree::ExpList({ new tree::ConstExp(recordByteSize) }));
  temp::Temp *recordReg = temp::TempFactory::NewTemp();
  tree::Stm *moveRecordToReg = new tree::MoveStm(
    new tree::TempExp(recordReg), callAllocExp);

  tree::Stm *moveFields = nullptr;
  tree::SeqStm *recurStm = nullptr;
  int offset = 0;
  auto absynFieldList = fields_->GetList();

  if (absynFieldList.size() == 1) {
    auto onlyExp = fields_->GetList().front()->exp_;
    moveFields = new tree::MoveStm(
      new tree::MemExp(new tree::TempExp(recordReg)),
      onlyExp->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx()
    );
  }

  else for (auto field : absynFieldList) {
    /* do necessary translate first */
    tr::ExpAndTy *fieldRes = field->exp_->Translate(venv, tenv, level, label, errormsg);

    /* calculate memory position of field */
    tree::Exp *fieldPos = nullptr;
    if (offset != 0) fieldPos = new tree::BinopExp(tree::PLUS_OP,
      new tree::TempExp(recordReg), new tree::ConstExp(offset * frame::WORD_SIZE));
    /* optimize: first element no need to use binop */
    else fieldPos = new tree::TempExp(recordReg);

    /* move field value from reg to its memory position */
    tree::Stm *moveReg = new tree::MoveStm(
      new tree::MemExp(fieldPos), fieldRes->exp_->UnEx());
    if (field == absynFieldList.front()) {
      /* first stm as left subtree */
      moveFields = new tree::SeqStm(moveReg, nullptr);
      recurStm = static_cast<tree::SeqStm*>(moveFields);
    } else if (field == absynFieldList.back()) {
      /* last stm no no need to create subtree*/
      recurStm->right_ = moveReg;
    } else {
      /* do the recursion */
      recurStm->right_ = new tree::SeqStm(moveReg, nullptr);
      recurStm = static_cast<tree::SeqStm*>(recurStm->right_);
    }
    offset++;
  }

  tree::Stm *finalStm = new tree::SeqStm(moveRecordToReg, moveFields);
  return new tr::ExpAndTy(
    new tr::ExExp(new tree::EseqExp(finalStm, new tree::TempExp(recordReg))),
    static_cast<type::RecordTy*>(tenv->Look(typ_)));

}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  auto absynExpList = seq_->GetList();

  if (absynExpList.size() == 1) {
    return absynExpList.front()->Translate(venv, tenv, level, label, errormsg);
  }

  type::Ty *resultTy = type::VoidTy::Instance();
  tree::EseqExp *eseqExp = nullptr;
  tree::EseqExp *recurExp = nullptr;
  for (auto exp : absynExpList) {
    tr::ExpAndTy *res = exp->Translate(venv, tenv, level, label, errormsg);
    if (exp == absynExpList.front()) {
      /* first element */
      eseqExp = new tree::EseqExp(res->exp_->UnNx(), nullptr);
      recurExp = eseqExp;
    } else if (exp == absynExpList.back()) {
      /* last element */
      recurExp->exp_ = res->exp_->UnEx();
      resultTy = res->ty_;
    } else {
      recurExp->exp_ = new tree::EseqExp(res->exp_->UnNx(), nullptr);
      recurExp = static_cast<tree::EseqExp*>(recurExp->exp_);
    }
  }
  return new tr::ExpAndTy(new tr::ExExp(eseqExp), resultTy);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,                       
                                   err::ErrorMsg *errormsg) const {
  tree::Stm *assignStm = new tree::MoveStm(
    var_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx(),
    exp_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx());
  return new tr::ExpAndTy(new tr::NxExp(assignStm), type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *testRes = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *thenRes = then_->Translate(venv, tenv, level, label, errormsg);

  temp::Label *trueLabel = temp::LabelFactory::NewLabel();
  temp::Label *falseLabel = temp::LabelFactory::NewLabel();
  tr::Cx testCx = testRes->exp_->UnCx(errormsg);
  /* do patch: is this right? */
  *testCx.trues_ = trueLabel, *testCx.falses_ = falseLabel;
  if (elsee_ != nullptr) {
    tr::ExpAndTy *elseRes = elsee_->Translate(venv, tenv, level, label, errormsg);
    temp::Temp *resultReg = temp::TempFactory::NewTemp();
    temp::Label *endLabel = temp::LabelFactory::NewLabel();

    /* tidy format! */
    tree::Exp *expSeq = 
      new tree::EseqExp(testCx.stm_, 
      new tree::EseqExp(new tree::LabelStm(trueLabel),
      new tree::EseqExp(new tree::MoveStm(new tree::TempExp(resultReg), thenRes->exp_->UnEx()),
      new tree::EseqExp(tree::JumpStm::ToLabel(endLabel),
      new tree::EseqExp(new tree::LabelStm(falseLabel),
      new tree::EseqExp(new tree::MoveStm(new tree::TempExp(resultReg), elseRes->exp_->UnEx()),
      new tree::EseqExp(tree::JumpStm::ToLabel(endLabel),
      new tree::EseqExp(new tree::LabelStm(endLabel),
      new tree::TempExp(resultReg)))))))));

    return new tr::ExpAndTy(new tr::ExExp(expSeq), thenRes->ty_->ActualTy());
  } else {
    /* no else part: must return nothing, much easier */
    tree::Stm *stmSeq = 
      new tree::SeqStm(testCx.stm_,
      new tree::SeqStm(new tree::LabelStm(trueLabel),
      new tree::SeqStm(thenRes->exp_->UnNx(),
      new tree::LabelStm(falseLabel))));

    return new tr::ExpAndTy(new tr::NxExp(stmSeq), type::VoidTy::Instance());
  }

}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,            
                                  err::ErrorMsg *errormsg) const {
  temp::Label *testLabel = temp::LabelFactory::NewLabel();
  temp::Label *bodyLabel = temp::LabelFactory::NewLabel();
  temp::Label *doneLabel = temp::LabelFactory::NewLabel();

  tr::ExpAndTy *testRes = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *bodyRes = body_->Translate(venv, tenv, level, doneLabel, errormsg);
  tr::Cx testCx = testRes->exp_->UnCx(errormsg);
  *testCx.trues_ = bodyLabel, *testCx.falses_ = doneLabel;

  tree::Stm *stmSeq = 
    new tree::SeqStm(new tree::LabelStm(testLabel),
    new tree::SeqStm(testCx.stm_,
    new tree::SeqStm(new tree::LabelStm(bodyLabel),
    new tree::SeqStm(bodyRes->exp_->UnNx(),
    new tree::SeqStm(tree::JumpStm::ToLabel(testLabel),
    new tree::LabelStm(doneLabel))))));

  return new tr::ExpAndTy(new tr::NxExp(stmSeq), type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *lowRes = lo_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *highRes = hi_->Translate(venv, tenv, level, label, errormsg);

  venv->BeginScope();
  tenv->BeginScope();

  /* loop variable can't escape, and is readonly */
  tr::Access *loopVar = tr::Access::AllocLocal(level, false);
  venv->Enter(var_, new env::VarEntry(loopVar, type::IntTy::Instance(), true));

  temp::Label *incrLabel = temp::LabelFactory::NewLabel();
  temp::Label *bodyLabel = temp::LabelFactory::NewLabel();
  temp::Label *doneLabel = temp::LabelFactory::NewLabel();
  tr::ExpAndTy *bodyRes = body_->Translate(venv, tenv, level, doneLabel, errormsg);

  tree::Stm *lessOrEqualTest = new tree::CjumpStm(tree::LE_OP, 
    AccessToExp(loopVar, reg_manager), highRes->exp_->UnEx(), bodyLabel, doneLabel);
  tree::Stm *lessThanTest = new tree::CjumpStm(tree::LT_OP, 
    AccessToExp(loopVar, reg_manager), highRes->exp_->UnEx(), incrLabel, doneLabel);

  tree::Stm *selfIncr = new tree::MoveStm(
    AccessToExp(loopVar, reg_manager),
    new tree::BinopExp(tree::PLUS_OP, AccessToExp(loopVar, reg_manager), new tree::ConstExp(1)));

  /* prevent from overflow */
  tree::Stm *stmSeq = 
    new tree::SeqStm(new tree::MoveStm(AccessToExp(loopVar, reg_manager), lowRes->exp_->UnEx()),
    new tree::SeqStm(lessOrEqualTest,
    new tree::SeqStm(new tree::LabelStm(bodyLabel),
    new tree::SeqStm(bodyRes->exp_->UnNx(),
    new tree::SeqStm(lessThanTest,
    new tree::SeqStm(new tree::LabelStm(incrLabel),
    new tree::SeqStm(selfIncr, 
    new tree::SeqStm(tree::JumpStm::ToLabel(bodyLabel),
    new tree::LabelStm(doneLabel)))))))));
  
  tenv->EndScope();
  venv->EndScope();

  return new tr::ExpAndTy(new tr::NxExp(stmSeq), type::VoidTy::Instance());
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(
    new tr::NxExp(tree::JumpStm::ToLabel(label)),
    type::VoidTy::Instance()
  );
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  tree::Exp* allExps = nullptr;
  tree::EseqExp* recurExp = nullptr;
  auto absynDecList = decs_->GetList();

  venv->BeginScope();
  tenv->BeginScope();

  /*
    Structure:
    allExps: decExp1 -> decExp2 -> ... -> decExpN -> bodyExp

    so decexp is always at left subtree
  */

  /* translate decs and build seq tree */
  for (auto dec : absynDecList) {
    tr::Exp *res = dec->Translate(venv, tenv, level, label, errormsg);
    if (dec == absynDecList.front()) {
      allExps = new tree::EseqExp(res->UnNx(), nullptr);
      recurExp = static_cast<tree::EseqExp*>(allExps);
    } else {
      recurExp->exp_ = new tree::EseqExp(res->UnNx(), nullptr);
      recurExp = static_cast<tree::EseqExp*>(recurExp->exp_);
    }
  }

  /* translate body is simple */
  tr::ExpAndTy *bodyRes = body_->Translate(venv, tenv, level, label, errormsg);

  /* corner case: no dec */
  if (allExps == nullptr) allExps = bodyRes->exp_->UnEx();
  else recurExp->exp_ = bodyRes->exp_->UnEx();
  

  tenv->EndScope();
  venv->EndScope();

  return new tr::ExpAndTy(new tr::ExExp(allExps), bodyRes->ty_);
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,                    
                                  err::ErrorMsg *errormsg) const {
  tree::Exp *sizeExp = 
    size_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
  tr::ExpAndTy *initRes = init_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *arrayTy = typ_ ? 
    tenv->Look(typ_)->ActualTy() : new type::ArrayTy(initRes->ty_->ActualTy());
  /* external call init_array */
  tree::Exp *initArray = frame::ExternalCall(
    "init_array",
    new tree::ExpList({ sizeExp, initRes->exp_->UnEx() }));
  temp::Temp *arrayReg = temp::TempFactory::NewTemp();
  /* move the base addr to a reg */
  tree::Stm *moveAddr = new tree::MoveStm(new tree::TempExp(arrayReg), initArray);
  return new tr::ExpAndTy(
    /* addr of array's first elem stored in arrayReg */
    new tr::ExExp(new tree::EseqExp(moveAddr, new tree::TempExp(arrayReg))),
    arrayTy
  );
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* add function dec into venv, left body */
  for (auto funDec : functions_->GetList()) {
    temp::Label *funcName = temp::LabelFactory::NamedLabel(funDec->name_->Name());
    /* first arg is always escape: static link */
    auto escapeList = std::list<bool>({ true });
    for (auto param : funDec->params_->GetList()) {
      escapeList.push_back(param->escape_);
    }
    /* record inner function arg count to reserve enough frame space */
    level->frame_->InnerFuncArgCount(static_cast<int>(escapeList.size()));

    tr::Level *newLevel = tr::Level::NewLevel(level, funcName, escapeList);
    auto formalTyList = funDec->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *resultTy = 
      funDec->result_ ? tenv->Look(funDec->result_) : type::VoidTy::Instance();
    env::FunEntry *funEntry = 
      new env::FunEntry(newLevel, funDec->name_, formalTyList, resultTy);
    venv->Enter(funDec->name_, funEntry);
  }
  /* translate the body */
  for (auto funDec : functions_->GetList()) {
    auto formalList = funDec->params_->MakeFieldList(tenv, errormsg);
    auto funEntry = static_cast<env::FunEntry*>(venv->Look(funDec->name_));
    /* first access is static-link */
    auto accessItr = ++funEntry->level_->frame_->Formals().begin();
    // assert(funEntry->level_->frame_->Formals().size() == formalList->GetList().size());
    venv->BeginScope();
    for (auto field : formalList->GetList()) {
      auto varEntry = new env::VarEntry(
        new tr::Access(funEntry->level_, *accessItr), field->ty_);
      venv->Enter(field->name_, varEntry);
      ++accessItr;
    }
    /* Body_6 */
    tr::ExpAndTy *bodyRes = 
      funDec->body_->Translate(venv, tenv, funEntry->level_, funEntry->label_, errormsg);
    venv->EndScope();

    /* Epilogue_7 */
    tree::Stm *bodyStm = new tree::MoveStm(
      new tree::TempExp(reg_manager->ReturnValue()), bodyRes->exp_->UnEx()); 
    /* after procEntryExit1, Prologue_4 -> Epilogue_8 have done */
    bodyStm = frame::ProcEntryExit1(funEntry->level_->frame_, bodyStm);
    frags->PushBack(new frame::ProcFrag(bodyStm, funEntry->level_->frame_));
  }

  return new tr::ExExp(new tree::ConstExp(114514));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* initRes->ty_ may be NilTy */
  tr::ExpAndTy *initRes = init_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *realTy = typ_ ? tenv->Look(typ_)->ActualTy() : initRes->ty_;
  // assert(initRes->ty_->IsSameType(tenv->Look(typ_)));
  tr::Access *valAccess = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(valAccess, realTy));
  /* move value from init to access */
  return new tr::NxExp(new tree::MoveStm(
    AccessToExp(valAccess, reg_manager), initRes->exp_->UnEx()));
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* enter all type names first */
  for (auto dec : types_->GetList()) {
    tenv->Enter(dec->name_, new type::NameTy(dec->name_, nullptr));
  }
  /* then translate all dec */
  for (auto dec : types_->GetList()) {
    type::NameTy *tmpNameTy = static_cast<type::NameTy*>(tenv->Look(dec->name_));
    tmpNameTy->ty_ = dec->ty_->Translate(tenv, errormsg);
  }
  /* finally set real type */
  for (auto dec : types_->GetList()) {
    type::NameTy *tmpNameTy = static_cast<type::NameTy*>(tenv->Look(dec->name_));
    tenv->Set(dec->name_, tmpNameTy->ty_);
  }

  /* waht should I return? */
  return new tr::ExExp(new tree::ConstExp(114514));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  return new type::NameTy(name_, tenv->Look(name_));
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  return new type::ArrayTy(tenv->Look(array_));
}

} // namespace absyn
