// TODO(phlip9): might need this later

#include "WowObject.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using std::ostream;
using std::ostringstream;
using std::string;

namespace phlipbot
{
string WowObject::GetName() const { return ""; }

ObjectType WowObject::GetObjectType() const { return ObjectType::NONE; }

void WowObject::PrintToStream(ostream& os) const
{
  os << std::hex << std::setfill('0');
  os << "{ type: WowObject";
  os << ", guid: 0x" << std::setw(16) << guid;
  os << ", base_ptr: 0x" << std::setw(8) << base_ptr;
  os << " }";
}

string WowObject::ToString() const
{
  ostringstream oss;
  PrintToStream(oss);
  return oss.str();
}

ostream& operator<<(ostream& os, WowObject const* obj)
{
  obj->PrintToStream(os);
  return os;
}
}
