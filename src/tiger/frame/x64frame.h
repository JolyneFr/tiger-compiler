//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */
public:
  X64RegManager();
  ~X64RegManager();
  /* all registers */
  temp::TempList *Registers() const override ;
  /* registers used for passing args */
  temp::TempList *ArgRegs() const override ;
  /* caller saved registers */
  temp::TempList *CallerSaves() const override ;
  /* callee saved registers */
  temp::TempList *CalleeSaves() const override ;
  temp::TempList *ReturnSink() const override ;
  int WordSize() const { return 8; }
  temp::Temp *FramePointer() const override ;
  temp::Temp *StackPointer() const override ;
  temp::Temp *ReturnValue() const override ;

  static temp::Temp *rax;
  static temp::Temp *rbx;
  static temp::Temp *rcx;
  static temp::Temp *rdx;
  static temp::Temp *rsp;
  static temp::Temp *rbp;
  static temp::Temp *rsi;
  static temp::Temp *rdi;
  static temp::Temp *r8;
  static temp::Temp *r9;
  static temp::Temp *r10;
  static temp::Temp *r11;
  static temp::Temp *r12;
  static temp::Temp *r13;
  static temp::Temp *r14;
  static temp::Temp *r15;
  /* virtual register store frame-pointer */
  static temp::Temp *fp;

private:
  temp::TempList *registers_ = nullptr;
  temp::TempList *argRegs_ = nullptr;
  temp::TempList *callerSaves_ = nullptr;
  temp::TempList *calleeSaves_ = nullptr;
  temp::TempList *returnSink_ = nullptr;

};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
