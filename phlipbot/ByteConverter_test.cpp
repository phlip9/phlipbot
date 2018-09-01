#include "ByteConverter.hpp"

#include <stdint.h>

#include <doctest.h>

namespace phlipbot
{
namespace test
{
TEST_CASE("ByteConverter::to_le is idempotent")
{
  uint32_t num = 0x12345678;
  ByteConverter::to_le(num);
  ByteConverter::to_le(num);
  CHECK(num == 0x12345678);
}

TEST_CASE("ByteConverter::to_be is idempotent")
{
  uint32_t num = 0x12345678;
  ByteConverter::to_be(num);
  ByteConverter::to_be(num);
  CHECK(num == 0x12345678);
}

TEST_CASE("ByteConverter::to_le only does something when IS_BIG_ENDIAN")
{
  uint32_t num = 0x12345678;
  ByteConverter::to_le(num);

  if (IS_BIG_ENDIAN) {
    CHECK(num == 0x78563412);
  } else {
    CHECK(num == 0x12345678);
  }
}

TEST_CASE("ByteConverter::to_be only does something when !IS_BIG_ENDIAN")
{
  uint32_t num = 0x12345678;
  ByteConverter::to_be(num);

  if (!IS_BIG_ENDIAN) {
    CHECK(num == 0x78563412);
  } else {
    CHECK(num == 0x12345678);
  }
}
}
}
