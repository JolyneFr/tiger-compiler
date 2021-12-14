#include "tiger/frame/x64frame.h"
#include <sstream>

extern frame::RegManager *reg_manager;

namespace frame {

/* alloc x86_64 registers */
temp::Temp *X64RegManager::rax = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::rbx = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::rcx = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::rdx = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::rsp = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::rbp = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::rsi = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::rdi = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::r8 = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::r9 = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::r10 = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::r11 = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::r12 = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::r13 = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::r14 = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::r15 = temp::TempFactory::NewTemp();
temp::Temp *X64RegManager::fp = temp::TempFactory::NewTemp();

X64RegManager::X64RegManager() {
  registers_ = new temp::TempList({
    rax, rbx, rcx, rdx, rsp, rbp, rsi, rdi,
    r8, r9, r10, r11, r12, r13, r14, r15
  });
  argRegs_ = new temp::TempList({
    rdi, rsi, rdx, rcx, r8, r9
  });
  callerSaves_ = new temp::TempList({
    rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11
  });
  calleeSaves_ = new temp::TempList({
    rbx, rbp, r12, r13, r14, r15
  });
  returnSink_ = new temp::TempList({
    rsp, rax, rbx, rbp, r12, r13, r14, r15
  });

  regs_ = std::vector<temp::Temp*>(
    registers_->GetList().begin(), registers_->GetList().end());

  temp_map_->Enter(rax, new std::string("%rax"));
  temp_map_->Enter(rbx, new std::string("%rbx"));
  temp_map_->Enter(rcx, new std::string("%rcx"));
  temp_map_->Enter(rdx, new std::string("%rdx"));
  temp_map_->Enter(rsp, new std::string("%rsp"));
  temp_map_->Enter(rbp, new std::string("%rbp"));
  temp_map_->Enter(rsi, new std::string("%rsi"));
  temp_map_->Enter(rdi, new std::string("%rdi"));
  temp_map_->Enter(r8, new std::string("%r8"));
  temp_map_->Enter(r9, new std::string("%r9"));
  temp_map_->Enter(r10, new std::string("%r10"));
  temp_map_->Enter(r11, new std::string("%r11"));
  temp_map_->Enter(r12, new std::string("%r12"));
  temp_map_->Enter(r13, new std::string("%r13"));
  temp_map_->Enter(r14, new std::string("%r14"));
  temp_map_->Enter(r15, new std::string("%r15"));
}

X64RegManager::~X64RegManager() {
  delete registers_;
  delete argRegs_;
  delete callerSaves_;
  delete calleeSaves_;
  delete returnSink_;
}

temp::TempList *X64RegManager::Registers() const { return registers_; }
temp::TempList *X64RegManager::ArgRegs() const { return argRegs_; }
temp::TempList *X64RegManager::CallerSaves() const { return callerSaves_; }
temp::TempList *X64RegManager::CalleeSaves() const { return calleeSaves_; }
temp::TempList *X64RegManager::ReturnSink() const { return returnSink_; }

temp::Temp *X64RegManager::FramePointer() const { return fp; }
temp::Temp *X64RegManager::StackPointer() const { return rsp; }
temp::Temp *X64RegManager::ReturnValue() const { return rax; }


class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}

  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::MemExp(
      new tree::BinopExp( tree::PLUS_OP, 
        framePtr, new tree::ConstExp(offset)
      )
    );
  }
};


class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}

  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::TempExp(reg);
  }
};

class X64Frame : public Frame {

public:
  X64Frame(temp::Label *name, std::list<bool> formals);
  ~X64Frame() override;
  std::list<Access*> &Formals() override { return formals_; }
  uint32_t Size() const override { return frame_size_; }
  /* alloc new access in frame */
  Access* AllocLocal(bool escape) override;
  std::vector<tree::Stm*> &ShiftOfView() { return shift_of_view_; }
  temp::Label* Name() override { return name_; }
  void InnerFuncArgCount(int argCount) override ;
  void AllocStackArg(RegManager *rm) override ;
private:
  /* private data member */
  std::list<Access*> formals_;
  uint32_t frame_size_;
  temp::Label *name_;
  std::vector<tree::Stm*> shift_of_view_;
  int max_inner_func_arg_cnt_;

  /* private used function */
};

/* result of Prologue_4 are saved in shift_of_view_ */
X64Frame::X64Frame(temp::Label *name, std::list<bool> formals): 
  name_(name), frame_size_(0), shift_of_view_(), max_inner_func_arg_cnt_(-1) {
  
  /* alloc space for formal params, and generate stm to move them */
  auto argRegs = reg_manager->ArgRegs()->GetList();
  auto argRegItr = argRegs.begin();
  /* start from origin stackPointer */
  int argIdx = 1, regArgCount = static_cast<int>(argRegs.size());
  for (const bool escape : formals) {
    frame::Access *curAccess = AllocLocal(escape);
    formals_.push_back(curAccess);
    tree::Exp *dstExp, *srcExp;
    /* calculate destination */
    dstExp = curAccess->ToExp(new tree::TempExp(reg_manager->FramePointer()));
    /* calculate source */
    if (argRegItr != argRegs.end()) {
      /* args from register */
      srcExp = new tree::TempExp(*argRegItr);
      argRegItr++;
    } else {
      /* args from prev frame */
      assert(argIdx > regArgCount);
      srcExp = new tree::MemExp(new tree::BinopExp(
        tree::PLUS_OP, 
        new tree::TempExp(reg_manager->FramePointer()),
        new tree::ConstExp((argIdx - regArgCount) * frame::WORD_SIZE)));
    }
    /* move from src to dst */
    shift_of_view_.push_back(new tree::MoveStm(dstExp, srcExp));
    argIdx++;
  }
}

/* pre-do Prologue_4 */
Frame *NewFrame(temp::Label *name, std::list<bool> formals) {
  return new X64Frame(name, formals);
}

/* do Prologue_4 & 5 & Epilogue_8, stm is translated body (6 & 7)*/
tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *body) {
  /* Prlogue_4 */
  auto x64Frame = static_cast<X64Frame*>(frame);
  tree::SeqStm *shiftRoot = nullptr;
  tree::SeqStm *shiftRecurStm = nullptr;
  for (auto shiftStm : x64Frame->ShiftOfView()) {
    if (shiftRoot == nullptr) {
      shiftRoot = new tree::SeqStm(shiftStm, nullptr);
      shiftRecurStm = shiftRoot;
    } else {
      shiftRecurStm->right_ = new tree::SeqStm(shiftStm, nullptr);
      shiftRecurStm = static_cast<tree::SeqStm*>(shiftRecurStm->right_);
    }
  }

  std::vector<frame::Access*> savedRegAccess;
  /* Prologue_5 */
  tree::SeqStm *saveRoot = nullptr;
  tree::SeqStm *saveRecurStm = nullptr;
  for (auto calleeSave : reg_manager->CalleeSaves()->GetList()) {
    frame::Access *reg = frame->AllocLocal(false);
    savedRegAccess.push_back(reg);
    tree::Exp *dstExp, *srcExp;

    dstExp = reg->ToExp(new tree::TempExp(reg_manager->FramePointer()));
    srcExp = new tree::TempExp(calleeSave);
    tree::Stm *moveStm = new tree::MoveStm(dstExp, srcExp);

    if (saveRoot == nullptr) {
      saveRoot = new tree::SeqStm(moveStm, nullptr);
      saveRecurStm = saveRoot;
    } else {
      saveRecurStm->right_ = new tree::SeqStm(moveStm, nullptr);
      saveRecurStm = static_cast<tree::SeqStm*>(saveRecurStm->right_);
    }
  }

  /* Epilogue_8 */
  tree::SeqStm *loadRoot = nullptr;
  tree::SeqStm *loadRecurStm = nullptr;
  /* load callee-saved registers reversely */
  auto reverseRegItr = reg_manager->CalleeSaves()->GetList().rbegin();
  auto reverseAccessItr = savedRegAccess.rbegin();
  while (reverseAccessItr != savedRegAccess.rend()) {
    tree::Exp *dstExp, *srcExp;

    dstExp = new tree::TempExp(*reverseRegItr);
    srcExp = (*reverseAccessItr)->ToExp(new tree::TempExp(reg_manager->FramePointer()));
    tree::Stm *moveStm = new tree::MoveStm(dstExp, srcExp);

    if (loadRoot == nullptr) {
      loadRoot = new tree::SeqStm(moveStm, nullptr);
      loadRecurStm = loadRoot;
    } else if (*reverseAccessItr == savedRegAccess.front()) {
      /* avoid nullptr in SeqStm */
      loadRecurStm->right_ = moveStm;
    } else {
      loadRecurStm->right_ = new tree::SeqStm(moveStm, nullptr);
      loadRecurStm = static_cast<tree::SeqStm*>(loadRecurStm->right_);
    }

    reverseRegItr++; reverseAccessItr++;
  }

  /* combine them all */
  shiftRecurStm->right_ = saveRoot;
  saveRecurStm->right_ = body;
  tree::SeqStm *entryExit1Root = new tree::SeqStm(shiftRoot, loadRoot);

  return entryExit1Root;
}

/* for regalloc */
assem::InstrList *ProcEntryExit2(assem::InstrList *body) {
  /* return sink as src: must live-out */
  body->Append(new assem::OperInstr("", new temp::TempList(), 
    reg_manager->ReturnSink(), nullptr));
  
  return body;
}

/* Prologue_1 & 2 & 3 & Epilogue_9 & 10 & 11 */
assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body) {
  std::stringstream prologue, epilogue;
  std::string funcName = frame->Name()->Name();
  std::string stackPtrName = *(reg_manager->temp_map_->Look(reg_manager->StackPointer()));
  /* fianlly, alloc space for arg */
  frame->AllocStackArg(reg_manager);

  prologue << "  .set " << tree::fsPlaceHolder(funcName) << ", " << frame->Size() << "\n";
  prologue << funcName << ":\n";
  prologue << "\tsubq $" << frame->Size() << ", " << stackPtrName << "\n";

  epilogue << "\taddq $" << frame->Size() << ", " << stackPtrName << "\n";
  epilogue << "\tretq\n";

  return new assem::Proc(prologue.str(), body, epilogue.str());
}

X64Frame::~X64Frame() {
  
}

Access* X64Frame::AllocLocal(bool escape) {
  if (escape) {
    /* escape indicates this access must store in frame */
    frame_size_ += WORD_SIZE;
    return new InFrameAccess(-frame_size_);
  } else {
    return new InRegAccess(temp::TempFactory::NewTemp());
  }
}

tree::Exp *ExternalCall(std::string_view name, tree::ExpList *args) {
  return new tree::CallExp(
    new tree::NameExp(temp::LabelFactory::NamedLabel(name)), args);
}

void X64Frame::InnerFuncArgCount(int argCount) {
  if (argCount > max_inner_func_arg_cnt_) {
    max_inner_func_arg_cnt_ = argCount;
  }
}

void X64Frame::AllocStackArg(RegManager *rm) {
  int rmArgRegCount = rm->ArgRegs()->GetList().size();
  if (max_inner_func_arg_cnt_ > rmArgRegCount) {
    frame_size_ += (max_inner_func_arg_cnt_ - rmArgRegCount) * frame::WORD_SIZE;
  }
}

} // namespace frame