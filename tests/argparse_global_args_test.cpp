#include "common.h"

#include "argparse/argparse.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

ARGPARSE_DECLARE_GLOBAL_FLAG(boolean);
ARGPARSE_DECLARE_GLOBAL_ARG(int, integer);
ARGPARSE_DECLARE_GLOBAL_MULTIARG(double, doubles);

namespace argparse {
namespace {

TEST(Parser, GlobalArgs) {
  Parser parser;
  ASSERT_RUNTIME_ERROR(parser.AddFlag("boolean"),
                       "Argument is already defined");
  ASSERT_RUNTIME_ERROR(parser.AddFlag("integer"),
                       "Argument is already defined");
  ASSERT_RUNTIME_ERROR(parser.AddFlag("doubles"),
                       "Argument is already defined");
  parser.ParseArgs({"binary", "-bi", "42", "-d", "2.71", "-d", "3.14"});
  EXPECT_TRUE(*::boolean);
  ASSERT_TRUE(::integer);
  EXPECT_EQ(*::integer, 42);
  EXPECT_THAT(::doubles.Values(),
              ::testing::ElementsAre(::testing::DoubleEq(2.71),
                                     ::testing::DoubleEq(3.14)));
}

}  // namespace
}  // namespace argparse