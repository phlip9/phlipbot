// TODO(phlip9): might need this later

#include "WowObject.hpp"

#include <string>

using namespace phlipbot::types;

namespace phlipbot
{
std::string WowObject::GetName() { return ""; }

ObjectType WowObject::GetObjectType() const { return ObjectType::NONE; }
}