#include "tiger/frame/temp.h"

#include <cstdio>
#include <set>
#include <sstream>

namespace temp {

LabelFactory LabelFactory::label_factory;
TempFactory TempFactory::temp_factory;

Label *LabelFactory::NewLabel() {
  char buf[100];
  sprintf(buf, "L%d", label_factory.label_id_++);
  return NamedLabel(std::string(buf));
}

/**
 * Get symbol of a label_. The label_ will be created only if it is not found.
 * @param s label_ string
 * @return symbol
 */
Label *LabelFactory::NamedLabel(std::string_view s) {
  return sym::Symbol::UniqueSymbol(s);
}

std::string LabelFactory::LabelString(Label *s) { return s->Name(); }

Temp *TempFactory::NewTemp() {
  Temp *p = new Temp(temp_factory.temp_id_++);
  std::stringstream stream;
  stream << 't';
  stream << p->num_;
  Map::Name()->Enter(p, new std::string(stream.str()));

  return p;
}

int Temp::Int() const { return num_; }

Map *Map::Empty() { return new Map(); }

Map *Map::Name() {
  static Map *m = nullptr;
  if (!m)
    m = Empty();
  return m;
}

Map *Map::LayerMap(Map *over, Map *under) {
  if (over == nullptr)
    return under;
  else
    return new Map(over->tab_, LayerMap(over->under_, under));
}

void Map::Enter(Temp *t, std::string *s) {
  assert(tab_);
  tab_->Enter(t, s);
}

std::string *Map::Look(Temp *t) {
  std::string *s;
  assert(tab_);
  s = tab_->Look(t);
  if (s)
    return s;
  else if (under_)
    return under_->Look(t);
  else
    return nullptr;
}

void Map::DumpMap(FILE *out) {
  tab_->Dump([out](temp::Temp *t, std::string *r) {
    fprintf(out, "t%d -> %s\n", t->Int(), r->data());
  });
  if (under_) {
    fprintf(out, "---------\n");
    under_->DumpMap(out);
  }
}

bool TempList::IsEquivalent(TempList *other) const {
  std::set<Temp*> selfSet(temp_list_.begin(), temp_list_.end());
  std::set<Temp*> otherSet(other->temp_list_.begin(), other->temp_list_.end());
  return selfSet == otherSet;
}

void TempList::CatList(TempList *other) {
  if (!other || other->GetList().empty()) return;
  temp_list_.insert(temp_list_.end(), 
    other->temp_list_.begin(), other->temp_list_.end());
}

bool TempList::Contain(Temp *target) const {
  for (auto temp : temp_list_ ) {
    if (temp == target) return true;
  }
  return false;
}

TempList *TempList::Union(TempList *tl) {
  auto res = new TempList();
  res->CatList(this);
  for (auto t : tl->GetList()) {
    if (!Contain(t)) res->Append(t);
  }
  return res;
}

TempList *TempList::Diff(TempList *tl) {
  auto res = new TempList();
  for (auto temp : temp_list_) {
    if (!tl->Contain(temp))
      res->temp_list_.push_back(temp);
  }
  return res;
}

void TempList::UnionWith(TempList *tl) {
  for (auto t : tl->temp_list_) {
    if (!Contain(t))
      temp_list_.push_back(t);
  }
}

void TempList::Replace(Temp *before, Temp *after) {
  auto itr = temp_list_.begin();
  while (itr != temp_list_.end()) {
    if (*itr == before) *itr = after;
    itr++;
  }
}

} // namespace temp