#include "argparse/argparse.h"

#include "common.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace argparse {

struct IntPair {
  int x;
  int y;
};

template <>
class TypeTraits<IntPair> {
public:
  static IntPair Cast(const std::string& str) {
    auto pos = str.find(',');
    return {std::stoi(str.substr(0, pos)), std::stoi(str.substr(pos + 1))};
  }

  static bool Equal(const IntPair& a, const IntPair& b) {
    return a.x == b.x && a.y == b.y;
  }
};

namespace {

TEST(Parser, Basic) {
  Parser parser;
  auto int1 = parser.AddArg<int>("integer1");
  auto int2 = parser.AddArg<int>("integer2", 'i');
  auto int3 = parser.AddArg<int>("integer3").Default(-1);
  auto int4 = parser.AddArg<int>("integer4");
  auto bool1 = parser.AddFlag("boolean1");
  auto bool2 = parser.AddFlag("boolean2");
  auto doubles = parser.AddMultiArg<double>("doubles", 'd');
  parser.ParseArgs({"binary", "--integer1", "42", "-i", "-2147483648",
                    "--boolean1", "--doubles", "3.14", "-d", "2.71"});

  ASSERT_TRUE(int1);
  EXPECT_EQ(*int1, 42);
  ASSERT_TRUE(int2);
  EXPECT_EQ(*int2, -2147483648);
  ASSERT_TRUE(int3);
  EXPECT_FALSE(int4);
  EXPECT_EQ(*int3, -1);
  EXPECT_TRUE(*bool1);
  EXPECT_FALSE(*bool2);
  EXPECT_THAT(*doubles, ::testing::ElementsAre(::testing::DoubleEq(3.14),
                                               ::testing::DoubleEq(2.71)));
}

TEST(Parser, ShortOptions) {
  {
    Parser parser;
    auto flag1 = parser.AddFlag("flag1", 'a');
    auto flag2 = parser.AddFlag("flag2", 'b');
    auto flag3 = parser.AddFlag("flag3", 'd');
    auto integer = parser.AddArg<int>("int", 'c');

    parser.ParseArgs({"binary", "-abc", "42"});
    EXPECT_TRUE(*flag1);
    EXPECT_TRUE(*flag2);
    EXPECT_FALSE(*flag3);
    EXPECT_EQ(*integer, 42);
  }
}

TEST(Parser, ArgWithDash) {
  Parser parser;
  auto strings = parser.AddMultiArg<std::string>("string");
  parser.ParseArgs({"binary", "--string=--double-dash", "--string",
                    "-dash=with=equal=signs"});
  EXPECT_THAT(*strings, ::testing::ElementsAre("--double-dash",
                                               "-dash=with=equal=signs"));
}

TEST(Parser, FreeArgs) {
  {
    Parser parser;
    ASSERT_ARGPARSE_ERROR(parser.ParseArgs({"binary", "free_arg"}),
                          "Free arguments are not enabled");
  }
  {
    Parser parser;
    parser.EnableFreeArgs();
    parser.ParseArgs(
        {"binary", "\\-free-arg", "\\--free-arg", "\\---free-arg"});
    EXPECT_THAT(
        parser.FreeArgs(),
        ::testing::ElementsAre("-free-arg", "--free-arg", "---free-arg"));
  }
  {
    Parser parser;
    parser.EnableFreeArgs();
    auto integer = parser.AddArg<int>("integer");
    parser.ParseArgs(
        std::vector<std::string>{"binary", "--integer", "5", "free_arg"});
    EXPECT_THAT(parser.FreeArgs(), ::testing::ElementsAre("free_arg"));
    EXPECT_TRUE(integer);
    EXPECT_EQ(*integer, 5);
  }
}

TEST(Parser, Options) {
  {
    Parser parser;
    parser.AddArg<int>("integer").Options({1, 2});
    ASSERT_ARGPARSE_ERROR(parser.ParseArgs({"binary", "--integer", "5"}),
                          "Provided argument string casts to an illegal value");
  }
  {
    Parser parser;
    parser.AddArg<int>("integer").Options({1, 2});
    ASSERT_NO_THROW(parser.ParseArgs({"binary"}));
  }
  {
    Parser parser;
    auto integer = parser.AddArg<int>("integer").Options({1, 2});
    parser.ParseArgs({"binary", "--integer", "1"});
    ASSERT_TRUE(integer);
    EXPECT_EQ(*integer, 1);
  }
}

TEST(Parser, Required) {
  Parser parser;
  parser.AddArg<int>("integer").Required();
  ASSERT_ARGPARSE_ERROR(parser.ParseArgs({"binary"}),
                        "No value provided for option");
}

TEST(Parser, ConfigsIncompatbility) {
  {
    Parser parser;
    ASSERT_ARGPARSE_ERROR(parser.AddArg<int>("integer").Required().Default(5),
                          "Required argument can't have a default value");
  }
  {
    Parser parser;
    ASSERT_ARGPARSE_ERROR(parser.AddArg<int>("integer").Default(5).Required(),
                          "Argument with a default value can't be required");
  }
  {
    Parser parser;
    ASSERT_ARGPARSE_ERROR(
        parser.AddMultiArg<int>("integer").Required().Default({5}),
        "Required argument can't have a default value");
  }
  {
    Parser parser;
    ASSERT_ARGPARSE_ERROR(
        parser.AddMultiArg<int>("integer").Default({5}).Required(),
        "Argument with a default value can't be required");
  }
}

TEST(Parser, CustomType) {
  {
    Parser parser;
    auto integers = parser.AddArg<IntPair>("integers");
    parser.ParseArgs({"binary", "--integers", "1,2"});
    EXPECT_EQ(integers->x, 1);
    EXPECT_EQ(integers->y, 2);
  }
  {
    Parser parser;
    auto integers = parser.AddArg<IntPair>("integers").Options({{1,2}});
    parser.ParseArgs({"binary", "--integers", "1,2"});
    EXPECT_EQ(integers->x, 1);
    EXPECT_EQ(integers->y, 2);
  }
}

TEST(Parser, PositionalArgs) {
  Parser parser;
  parser.EnableFreeArgs();
  auto string = parser.AddPositionalArg<std::string>();
  auto integer = parser.AddPositionalArg<int>();
  parser.ParseArgs(
      {"binary", "\\--number", "64", "free", "args", "go", "here"});
  ASSERT_TRUE(string);
  EXPECT_EQ(*string, "--number");
  ASSERT_TRUE(integer);
  EXPECT_EQ(*integer, 64);
  EXPECT_THAT(parser.FreeArgs(),
              ::testing::ElementsAre("free", "args", "go", "here"));
}

TEST(Parser, MultiplePositionalArgs) {
  Parser parser;
  auto [string, integer, number] =
      parser.AddPositionalArgs<std::string, int, double>();
  parser.EnableFreeArgs();
  parser.ParseArgs(
      {"binary", "\\--number", "64", "3.14", "free", "args", "go", "here"});
  ASSERT_TRUE(string);
  EXPECT_EQ(*string, "--number");
  ASSERT_TRUE(integer);
  EXPECT_EQ(*integer, 64);
  ASSERT_TRUE(number);
  EXPECT_THAT(*number, ::testing::DoubleEq(3.14));
  EXPECT_THAT(parser.FreeArgs(),
              ::testing::ElementsAre("free", "args", "go", "here"));
}

TEST(Parser, BigExample) {
  Parser parser;
  parser.EnableFreeArgs();

  auto command = parser.AddPositionalArg<std::string>().Required();
  auto rm = parser.AddFlag("rm");
  auto interactive = parser.AddFlag("interactive", 'i');
  auto tty = parser.AddFlag("tty", 't');
  auto verbose = parser.AddFlag("verbose", 'v');
  auto jobs = parser.AddArg<int>("jobs", 'j').Required();
  auto name = parser.AddArg<std::string>("name").Required();
  auto use_something = parser.AddArg<bool>("use-something").Required();
  auto use_something_else =
      parser.AddArg<bool>("use-something-else").Required();
  auto errors = parser.AddFlag("errors", 'e');
  auto trace = parser.AddFlag("trace", 'x');
  auto shell_option = parser.AddArg<std::string>("shell-option", 'o');
  auto [floating_point, integer, str] =
      parser.AddPositionalArgs<double, int, std::string>();

  auto unused_and_unset_boolean = parser.AddArg<bool>("unused-boolean");

  parser.ParseArgs({"binary", "run", "--rm", "-it", "-vvv", "-j4", "--name",
                    "name", "--use-something=false",
                    "--use-something-else=true", "-eo", "pipefail", "2.5", "42",
                    "\\--something-with-leading-dashes",
                    "will-not-match-anything"});

  EXPECT_EQ(*command, "run");
  EXPECT_TRUE(*rm);
  EXPECT_TRUE(*interactive);
  EXPECT_TRUE(*tty);
  EXPECT_EQ(*verbose, 3);
  EXPECT_EQ(*jobs, 4);
  EXPECT_EQ(*name, "name");
  EXPECT_FALSE(*use_something);
  EXPECT_TRUE(*use_something_else);
  EXPECT_TRUE(*errors);
  EXPECT_FALSE(*trace);
  ASSERT_TRUE(shell_option);
  EXPECT_EQ(*shell_option, "pipefail");
  ASSERT_TRUE(floating_point);
  EXPECT_EQ(*floating_point, 2.5f);
  ASSERT_TRUE(integer);
  EXPECT_EQ(*integer, 42);
  ASSERT_TRUE(str);
  EXPECT_EQ(*str, "--something-with-leading-dashes");
  EXPECT_THAT(parser.FreeArgs(),
              ::testing::ElementsAre("will-not-match-anything"));
  EXPECT_FALSE(unused_and_unset_boolean);
}

}  // namespace
}  // namespace argparse
