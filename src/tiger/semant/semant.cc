#include "tiger/absyn/absyn.h"
#include "tiger/semant/semant.h"
#include <algorithm>
#include <vector>

bool existSymbol(std::vector<sym::Symbol*> vec, sym::Symbol* symbol) {
  return std::find(vec.cbegin(), vec.cend(), symbol) != vec.cend();
}

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    return static_cast<env::VarEntry*>(entry)->ty_->ActualTy();
  } else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().c_str());
    return type::IntTy::Instance();
  }
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *lvalueType = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!lvalueType || typeid(*lvalueType) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }
  type::FieldList *fieldList = static_cast<type::RecordTy*>(lvalueType)->fields_;
  std::string symName = sym_->Name();
  for (const type::Field *field : fieldList->GetList()) {
    if (!symName.compare(field->name_->Name())) {
      return field->ty_->ActualTy();
    }
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().c_str());
  return type::IntTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *lvalueType = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!lvalueType || typeid(*lvalueType) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::IntTy::Instance();
  }
  type::Ty *expType = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!expType || typeid(*expType) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "array subscript must be integer");
    return type::IntTy::Instance();
  }
  return static_cast<type::ArrayTy*>(lvalueType)->ty_->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().c_str());
    return type::IntTy::Instance();
  }
  env::FunEntry *funcEntry = static_cast<env::FunEntry*>(entry);

  auto formalList = funcEntry->formals_->GetList();
  auto actualList = args_->GetList();
  auto formalItr = formalList.cbegin();
  auto actualItr = actualList.cbegin();
  while (formalItr != formalList.cend()) {
    /* actuals ends: too little args */
    if (actualItr == actualList.cend()) {
      errormsg->Error(pos_, "too little params in function %s", func_->Name().c_str());
      return type::IntTy::Instance();
    }
    type::Ty *actualType = (*actualItr)->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!actualType->IsSameType((*formalItr)->ActualTy())) {
      errormsg->Error(pos_, "para type mismatch");
      return type::IntTy::Instance();
    }
    formalItr++; actualItr++;
  }
  /* actuals not end: too many args */
  if (actualItr != actualList.cend()) {
    errormsg->Error(pos_, "too many params in function %s", func_->Name().c_str());
    return type::IntTy::Instance();
  }

  return funcEntry->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *leftType = left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *rightType = right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP 
      || oper_ == absyn::TIMES_OP || oper_ == absyn::DIVIDE_OP) {
    if (typeid(*leftType) != typeid(type::IntTy)) {
      errormsg->Error(left_->pos_, "integer required");
    }
    if (typeid(*rightType) != typeid(type::IntTy)) {
      errormsg->Error(right_->pos_, "integer required");
    }
    return type::IntTy::Instance();
  } else {
    if (!leftType->IsSameType(rightType)) {
      errormsg->Error(pos_, "same type required");
      return type::IntTy::Instance();
    }
  }

  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *rType = tenv->Look(typ_);
  if (!rType || typeid(*rType) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
    return type::IntTy::Instance();
  }

  type::RecordTy *recordType = static_cast<type::RecordTy*>(rType);
  auto fieldList = recordType->fields_->GetList();
  auto eFieldList = fields_->GetList();
  auto fieldItr = fieldList.cbegin();
  auto eFieldItr = eFieldList.cbegin();
  while (fieldItr != fieldList.cend()) {
    /* eField ends: */
    if (eFieldItr == eFieldList.cend()) {
      errormsg->Error(pos_, "too little fields");
      return type::IntTy::Instance();
    }
    /* field name mismatch */
    if ((*fieldItr)->name_->Name().compare((*eFieldItr)->name_->Name())) {
      errormsg->Error(pos_, "undefined field %s", (*eFieldItr)->name_->Name().c_str());
      return type::IntTy::Instance();
    }
    /* exp type mismatch */
    type::Ty *eType = (*eFieldItr)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!eType || !eType->IsSameType((*fieldItr)->ty_)) {
      errormsg->Error(pos_, "field type mismatch");
      return type::IntTy::Instance();
    }
    fieldItr++; eFieldItr++;
  }
  /* eField not end: */
  if (eFieldItr != eFieldList.cend()) {
      errormsg->Error(pos_, "too many fields");
      return type::IntTy::Instance();
  }

  return recordType->ActualTy();
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *resultType = type::VoidTy::Instance();
  for (const absyn::Exp *exp : seq_->GetList()) {
    resultType = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return resultType->ActualTy();
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *varType = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *expType = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!varType || !expType || !varType->IsSameType(expType)) {
    errormsg->Error(pos_, "unmatched assign exp");
    return type::IntTy::Instance();
  }

  if (typeid(*var_) == typeid(absyn::SimpleVar)) {
    absyn::SimpleVar *sVar = static_cast<absyn::SimpleVar*>(var_);
    env::EnvEntry *entry = venv->Look(sVar->sym_);
    if (entry->readonly_) {
      errormsg->Error(pos_, "loop variable can't be assigned");
      return type::IntTy::Instance();
    }
  }

  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *testType = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!testType || typeid(*testType) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "integer required");
    return type::IntTy::Instance();
  }
  type::Ty *thenType = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *elseType = elsee_ ? 
    elsee_->SemAnalyze(venv, tenv, labelcount, errormsg) : 
    nullptr;
  if (elseType) {
    if (!thenType->IsSameType(elseType)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
      return type::IntTy::Instance();
    }
  } else if (typeid(*thenType) != typeid(type::VoidTy)) {
    errormsg->Error(pos_, "if-then exp's body must produce no value");
    return type::IntTy::Instance();
  }

  return thenType->ActualTy();
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *testType = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!testType || typeid(*testType) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "integer required");
    return type::IntTy::Instance();
  }
  type::Ty *bodyType = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  if (!bodyType || typeid(*bodyType) != typeid(type::VoidTy)) {
    errormsg->Error(pos_, "while body must produce no value");
    return type::IntTy::Instance();
  }
  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *lowType = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *highType = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!lowType || typeid(*lowType) != typeid(type::IntTy)) {
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
  }
  if (!highType || typeid(*highType) != typeid(type::IntTy)) {
    errormsg->Error(hi_->pos_, "for exp's range type is not integer");
  }
  
  venv->BeginScope();
  tenv->BeginScope();
  venv->Enter(var_, new env::VarEntry(lowType, true));
  type::Ty *bodyType = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  tenv->EndScope();
  venv->EndScope();

  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if (!labelcount) {
    /* use labelcount to judge whether break is in loop  */
    errormsg->Error(pos_, "break is not inside any loop");
    return type::IntTy::Instance();
  }
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope();
  tenv->BeginScope();
  for (const absyn::Dec *dec : decs_->GetList()) {
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  type::Ty *bodyType = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  tenv->EndScope();
  venv->EndScope();

  return bodyType->ActualTy();
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *arrType = tenv->Look(typ_);
  if (!arrType) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
    return type::IntTy::Instance();
  }
  while (typeid(*arrType) == typeid(type::NameTy)) {
    arrType = static_cast<type::NameTy*>(arrType)->ty_;
  }
  if (typeid(*arrType) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "%s is not an array", typ_->Name().c_str());
    return type::IntTy::Instance();
  }
  type::ArrayTy *arrayType = static_cast<type::ArrayTy*>(arrType);

  type::Ty *sizeType = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!sizeType || typeid(*sizeType) != typeid(type::IntTy)) {
    errormsg->Error(size_->pos_, "integer required");
    return type::IntTy::Instance();
  }

  type::Ty *initType = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *elemType = arrayType->ty_;
  while (typeid(*elemType) == typeid(type::NameTy)) {
    elemType = static_cast<type::NameTy*>(elemType)->ty_;
  }
  if (!initType || !elemType->IsSameType(initType)) {
    errormsg->Error(size_->pos_, "type mismatch");
    return type::IntTy::Instance();
  }
  /* type of array's element */
  return arrType->ActualTy();
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  std::vector<sym::Symbol*> scopeFuncs;
  /* first round: only scan declaration */
  for (const FunDec* funDec : functions_->GetList()) {
    if (existSymbol(scopeFuncs, funDec->name_)) {
      errormsg->Error(pos_, "two functions have the same name");
    }
    type::TyList *formalList = funDec->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *resultType = funDec->result_ ? 
      tenv->Look(funDec->result_) : type::VoidTy::Instance();
    env::FunEntry *funEntry = new env::FunEntry(formalList, resultType);
    venv->Enter(funDec->name_, funEntry);
    scopeFuncs.push_back(funDec->name_);
  }
  /* second round: scan function body */
  for (const FunDec* funDec : functions_->GetList()) {
    type::FieldList *formalList = funDec->params_->MakeFieldList(tenv, errormsg);
    venv->BeginScope();
    for (const type::Field *field : formalList->GetList()) {
      venv->Enter(field->name_, new env::VarEntry(field->ty_));
    }
    type::Ty *bodyType = funDec->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    type::Ty *resultType = static_cast<env::FunEntry*>(venv->Look(funDec->name_))->result_;
    venv->EndScope();
    if (typeid(*bodyType) != typeid(*resultType)) {
      if (typeid(*resultType) == typeid(type::VoidTy)) {
        errormsg->Error(pos_, "procedure returns value");
      } else {
        errormsg->Error(pos_, "return type mismatch");
      }
    }
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *initType = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typ_) {
    type::Ty *varType = tenv->Look(typ_);
    if (!varType) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
      return;
    }
    if (!initType->IsSameType(varType)) {
      errormsg->Error(pos_, "type mismatch");
    } 
    venv->Enter(var_, new env::VarEntry(varType));
    
  } else {
    if (typeid(*initType) == typeid(type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
    } else {
      venv->Enter(var_, new env::VarEntry(initType));
    }
  }
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  std::vector<sym::Symbol*> scopeTypes;
  /* first round: only scan declaration */
  for (const absyn::NameAndTy *nameAndTy : types_->GetList()) {
    sym::Symbol *nameSym = nameAndTy->name_;
    if (existSymbol(scopeTypes, nameSym)) {
      errormsg->Error(pos_, "two types have the same name");
    }
    /* set nullptr to real Type later */
    tenv->Enter(nameSym, new type::NameTy(nameSym, nullptr));
    scopeTypes.push_back(nameSym);
  }
  /* second round: alloc real type */
  for (const absyn::NameAndTy *nameAndTy : types_->GetList()) {
    type::NameTy *tyType = static_cast<type::NameTy*>(tenv->Look(nameAndTy->name_));
    tyType->ty_ = nameAndTy->ty_->SemAnalyze(tenv, errormsg);
  }
  /* third round: detect cycle */
  for (const absyn::NameAndTy *nameAndTy : types_->GetList()) {
    type::NameTy *origType = static_cast<type::NameTy*>(tenv->Look(nameAndTy->name_));
    type::Ty *curType = origType->ty_;
    while (typeid(*curType) == typeid(type::NameTy) && curType != origType) {
      curType = static_cast<type::NameTy*>(curType)->ty_;
    }
    if (curType == origType) {
      errormsg->Error(pos_, "illegal type cycle");
      return;
    }
  }
  /* fourth round: assgin real type entry */
  for (const absyn::NameAndTy *nameAndTy : types_->GetList()) {
    type::NameTy *origType = static_cast<type::NameTy*>(tenv->Look(nameAndTy->name_));
    tenv->Set(nameAndTy->name_, origType->ty_);
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(name_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().c_str());
    return type::IntTy::Instance();
  }
  return new type::NameTy(name_, tenv->Look(name_));
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(array_);
  if (!ty) {
    errormsg->Error(pos_, "undefined array %s", array_->Name().c_str());
    return type::IntTy::Instance();
  }
  return new type::ArrayTy(ty);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
