#include "WowContainer.hpp"

#include <iomanip>

using std::ostream;

using phlipbot::offsets::Descriptor;

namespace phlipbot
{
uint32_t WowContainer::GetItemId() const
{
  return GetDescriptor<uint32_t>(Descriptor::ContainerId);
}

void WowContainer::PrintToStream(ostream& os) const
{
  os << std::hex << std::setfill('0');
  os << "{ type: WowContainer";
  os << ", guid: 0x" << std::setw(16) << guid;
  os << ", base_ptr: 0x" << std::setw(8) << base_ptr;
  os << ", name: " << GetName();
  os << std::dec;
  os << ", item_id: " << GetItemId();
  os << " }";
}

ObjectType WowContainer::GetObjectType() const { return ObjectType::CONTAINER; }
}