#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <list>
#include <memory>
#include <string>

#include "tiger/frame/temp.h"
#include "tiger/translate/tree.h"
#include "tiger/codegen/assem.h"


namespace frame {

class RegManager {
public:
  RegManager() : temp_map_(temp::Map::Empty()) {}
  ~RegManager() = default;

  temp::Temp *GetRegister(int regno) { return regs_[regno]; }

  /**
   * Get general-purpose registers except RSI
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  [[nodiscard]] virtual temp::TempList *Registers() const = 0;

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  [[nodiscard]] virtual temp::TempList *ArgRegs() const = 0;

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CallerSaves() const = 0;

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CalleeSaves() const = 0;

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  [[nodiscard]] virtual temp::TempList *ReturnSink() const = 0;

  /**
   * Get word size
   */
  [[nodiscard]] virtual int WordSize() const = 0;

  [[nodiscard]] virtual temp::Temp *FramePointer() const = 0;

  [[nodiscard]] virtual temp::Temp *StackPointer() const = 0;

  [[nodiscard]] virtual temp::Temp *ReturnValue() const = 0;

  temp::Map *temp_map_;
protected:
  std::vector<temp::Temp *> regs_;
};

class Access {
public:
  /* TODO: Put your lab5 code here */
  virtual tree::Exp *ToExp(tree::Exp *framePtr) const = 0;
  virtual ~Access() = default;
  
};

class Frame {
  /* TODO: Put your lab5 code here */
public:

  virtual ~Frame() {}

  [[nodiscard]] virtual uint32_t Size() const = 0;

  [[nodiscard]] virtual std::list<Access*> &Formals() = 0;

  [[nodiscard]] virtual Access* AllocLocal(bool escape) = 0;

  [[nodiscard]] virtual temp::Label* Name() = 0;

  virtual void InnerFuncArgCount(int argCount) = 0;

  virtual void AllocStackArg(RegManager *rm) = 0;

};

/*
  Function Defination
  --------------------------------------------------------------------
    Prologue:
    1. Pseudo-instructions to announce the beginning of a function;
    2. A label definition of the function name
    3. An instruction to adjust the stack pointer
    4. Instructions to save “escaping” arguments – including the static link – into the frame, and to move nonescaping arguments into fresh temporary registers
    5. Store instructions to save any callee-saved registers- including the return address register – used within the function.

    Body:
    6. Just do the function body.

    Epilogue:
    7. An instruction to move the return value (result of the function) to the register reserved for that purpose
    8. Load instructions to restore the callee-save registers
    9. An instruction to reset the stack pointer (to deallocate the frame)
    10. A return instruction (Jump to the return address)
    11. Pseduo-instructions, as needed, to announce the end of a function

*/

/* create a new X64Frame: pre-do Prologue_4 */
Frame *NewFrame(temp::Label *name, std::list<bool> formals);

/* do Prologue_4 & 5 & Epilogue_8 (stm include Body_6 & Epilogue_7) */
tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *stm);

assem::InstrList *ProcEntryExit2(assem::InstrList *body);

assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body);

/* call an external function */
tree::Exp* ExternalCall(std::string_view name, tree::ExpList *args);

static const int WORD_SIZE = 8;

/**
 * Fragments
 */

class Frag {
public:
  virtual ~Frag() = default;

  enum OutputPhase {
    Proc,
    String,
  };

  /**
   *Generate assembly for main program
   * @param out FILE object for output assembly file
   */
  virtual void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const = 0;
};

class StringFrag : public Frag {
public:
  temp::Label *label_;
  std::string str_;

  StringFrag(temp::Label *label, std::string str)
      : label_(label), str_(std::move(str)) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class ProcFrag : public Frag {
public:
  tree::Stm *body_;
  Frame *frame_;

  ProcFrag(tree::Stm *body, Frame *frame) : body_(body), frame_(frame) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class Frags {
public:
  Frags() = default;
  void PushBack(Frag *frag) { frags_.emplace_back(frag); }
  const std::list<Frag*> &GetList() { return frags_; }

private:
  std::list<Frag*> frags_;
};

/* TODO: Put your lab5 code here */

} // namespace frame

#endif