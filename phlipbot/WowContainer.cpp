#include "WowContainer.hpp"

#include "wow_constants.hpp"

using namespace phlipbot::types;

namespace phlipbot
{
ObjectType WowContainer::GetObjectType() const { return ObjectType::CONTAINER; }
}