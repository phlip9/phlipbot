#include "ByteBuffer.hpp"

#include <array>

#include <doctest.h>

// TODO(phlip9): Write more comprehensive tests.

namespace phlipbot
{
namespace test
{
TEST_CASE("ByteBuffer can put and get a uint32_t")
{
  uint32_t my_int = 0xD34DB33F;
  ByteBuffer buf{4};
  buf << my_int;

  uint32_t my_int2;
  buf >> my_int2;

  CHECK(my_int == my_int2);
}

TEST_CASE("ByteBuffer can put and get several uint32_t")
{
  std::array<uint32_t, 4> xs{1234, 9999, 0, 1111};

  ByteBuffer buf{xs.size() * sizeof(uint32_t)};

  for (auto x : xs) {
    buf << x;
  }

  for (auto x : xs) {
    uint32_t y;
    buf >> y;
    CHECK(x == y);
  }
}

TEST_CASE("ByteBuffer can put and get std::string")
{
  std::string foo{"foo bar baz"};
  ByteBuffer buf;
  buf << foo;
  std::string foo2;
  buf >> foo2;
  CHECK(foo == foo2);
}
}
}