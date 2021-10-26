#include "straightline/slp.h"

#include <iostream>

namespace A {

int Max(const int i1, const int i2);

int Op(const int left, BinOp oper, const int right);

int A::CompoundStm::MaxArgs() const {
  return Max(stm1 -> MaxArgs(), stm2 -> MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  return stm2 -> Interp(stm1 -> Interp(t));
}

int A::AssignStm::MaxArgs() const {
  return exp -> MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  IntAndTable *result = exp -> Interp(t);
  return result -> t -> Update(id, result -> i);
}

int A::PrintStm::MaxArgs() const {
  return Max(exps -> NumExps(), exps -> MaxArgs());
}

Table *A::PrintStm::Interp(Table *t) const {
  return exps -> InterpAndPrint(t) -> t;
}

int A::IdExp::MaxArgs() const {
  return 0;
}

IntAndTable *A::IdExp::Interp(Table *t) const {
  return new IntAndTable(t -> Lookup(id), t);
}

int A::NumExp::MaxArgs() const {
  return 0;
}

IntAndTable *A::NumExp::Interp(Table *t) const {
  return new IntAndTable(num, t);
}

int A::OpExp::MaxArgs() const {
  return Max(left -> MaxArgs(), right -> MaxArgs());
}

IntAndTable *A::OpExp::Interp(Table *t) const {
  IntAndTable *left_result = left -> Interp(t);
  IntAndTable *right_result = right -> Interp(left_result -> t);
  return new IntAndTable(
    Op(left_result -> i, oper, right_result -> i), 
    right_result -> t
  );
}

int A::EseqExp::MaxArgs() const {
  return Max(stm -> MaxArgs(), exp -> MaxArgs());
}

IntAndTable *A::EseqExp::Interp(Table *t) const {
  return exp -> Interp(stm -> Interp(t));
}

int A::PairExpList::MaxArgs() const {
  return Max(exp -> MaxArgs(), tail -> MaxArgs());
}

int A::PairExpList::NumExps() const {
  return 1 + tail -> NumExps();
}

IntAndTable *A::PairExpList::Interp(Table *t) const {
  IntAndTable *head_result = exp -> Interp(t);
  return tail -> Interp(head_result -> t);
}

IntAndTable *A::PairExpList::InterpAndPrint(Table *t) const {
  IntAndTable *head_result = exp -> Interp(t);
  std::cout << head_result -> i << " ";
  return tail -> InterpAndPrint(head_result -> t);
}

int A::LastExpList::MaxArgs() const {
  return exp -> MaxArgs();
}

int A::LastExpList::NumExps() const {
  return 1;
}

IntAndTable *A::LastExpList::Interp(Table *t) const {
  return exp -> Interp(t);
}

IntAndTable *A::LastExpList::InterpAndPrint(Table *t) const {
  IntAndTable *result = exp -> Interp(t);
  std::cout << result -> i << std::endl;
  return result;
}


int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}

int Max(const int i1, const int i2) {
  return i1 > i2 ? i1 : i2;
}

int Op(const int left, const BinOp oper, const int right) {
  switch (oper) {
    case PLUS: return left + right;
    case MINUS: return left - right;
    case TIMES: return left * right;
    case DIV: return left / right;
  }
}

}  // namespace A
