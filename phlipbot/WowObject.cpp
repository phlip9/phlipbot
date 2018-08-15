// TODO(phlip9): might need this later

#include "WowObject.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using namespace phlipbot::types;

namespace phlipbot
{
std::string WowObject::GetName() const { return ""; }

ObjectType WowObject::GetObjectType() const { return ObjectType::NONE; }

void WowObject::PrintToStream(std::ostream& os) const
{
  os << std::hex << std::setfill('0');
  os << "{ type: WowObject";
  os << ", guid: 0x" << std::setw(16) << guid;
  os << ", base_ptr: 0x" << std::setw(8) << base_ptr;
  os << " }";
}

std::string WowObject::ToString() const
{
  std::ostringstream oss;
  PrintToStream(oss);
  return oss.str();
}

std::ostream& operator<<(std::ostream& os, WowObject const* obj)
{
  obj->PrintToStream(os);
  return os;
}
}
