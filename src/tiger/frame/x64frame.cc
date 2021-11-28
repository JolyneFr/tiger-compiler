#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */
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
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::TempExp(reg);
  }
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  X64Frame(temp::Label *name, std::list<bool> formals);
  ~X64Frame() override;
  std::list<Access*> &Formals() override { return formals_; }
  uint32_t Size() const override { return frame_size_; }
  /* alloc new access in frame */
  Access* AllocLocal(bool escape) override;
private:
  /* private data member */
  std::list<Access*> formals_;
  uint32_t frame_size_;
  temp::Label *label_;

  /* private used function */
};
/* TODO: Put your lab5 code here */
X64Frame::X64Frame(temp::Label *name, std::list<bool> formals): label_(name) {
  /* todo */
}

Frame *NewFrame(temp::Label *name, std::list<bool> formals) {
  return new X64Frame(name, formals);
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

tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *stm) {
  /* TODO */
  return stm;
}

} // namespace frame