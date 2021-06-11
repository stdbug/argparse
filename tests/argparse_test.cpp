#include "argparse/argparse.h"

#include "common.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace argparse {

struct IntPair {
  int x;
  int y;
};

IntPair IntPairFromString(const std::string& str) {
  auto pos = str.find(',');
  return IntPair{std::stoi(str.substr(0, pos)), std::stoi(str.substr(pos + 1))};
}

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
  EXPECT_THAT(doubles.Values(),
              ::testing::ElementsAre(::testing::DoubleEq(3.14),
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
  {
    Parser parser;
    parser.EnableFreeArgs();
    auto flag = parser.AddFlag("flag", 'a');
    auto integer = parser.AddArg<int>("int", 'b');

    parser.ParseArgs({"binary", "-ba", "42"});
    EXPECT_FALSE(*flag);
    EXPECT_FALSE(integer);
    EXPECT_THAT(parser.FreeArgs(), ::testing::ElementsAre("-ba", "42"));
  }
}

TEST(Parser, ArgWithDash) {
  Parser parser;
  auto strings = parser.AddMultiArg<std::string>("string");
  parser.ParseArgs(
      {"binary", "--string=--double-dash", "--string=-dash=with=equal=signs"});
  EXPECT_THAT(strings.Values(), ::testing::ElementsAre(
                                    "--double-dash", "-dash=with=equal=signs"));
}

TEST(Parser, FreeArgs) {
  {
    Parser parser;
    ASSERT_RUNTIME_ERROR(parser.ParseArgs({"binary", "free_arg"}),
                         "Free arguments are not allowed");
  }
  {
    Parser parser;
    parser.EnableFreeArgs();
    ASSERT_NO_THROW(
        parser.ParseArgs({"binary", "-free-arg", "--free-arg", "---free-arg"}));
    EXPECT_THAT(
        parser.FreeArgs(),
        ::testing::ElementsAre("-free-arg", "--free-arg", "---free-arg"));
  }
  {
    Parser parser;
    parser.EnableFreeArgs();
    auto integer = parser.AddArg<int>("integer");
    ASSERT_NO_THROW(parser.ParseArgs(
        std::vector<std::string>{"binary", "--integer", "5", "free_arg"}));
    EXPECT_THAT(parser.FreeArgs(), ::testing::ElementsAre("free_arg"));
    EXPECT_TRUE(integer.HasValue());
    EXPECT_EQ(*integer, 5);
  }
}

TEST(Parser, Options) {
  {
    Parser parser;
    parser.AddArg<int>("integer").Options({1, 2});
    ASSERT_RUNTIME_ERROR(parser.ParseArgs({"binary", "--integer", "5"}),
                         "Provided argument casts to an illegal value");
  }
  {
    Parser parser;
    parser.AddArg<int>("integer").Options({1, 2});
    EXPECT_NO_THROW(parser.ParseArgs({"binary"}));
  }
  {
    Parser parser;
    auto integer = parser.AddArg<int>("integer").Options({1, 2});
    parser.ParseArgs({"binary", "--integer", "1"});
    EXPECT_TRUE(integer.HasValue());
    EXPECT_EQ(*integer, 1);
  }
}

TEST(Parser, ConfigsIncompatbility) {
  {
    Parser parser;
    ASSERT_RUNTIME_ERROR(parser.AddArg<int>("integer").Required().Default(5),
                         "Required argument can't have a default value");
  }
  {
    Parser parser;
    ASSERT_RUNTIME_ERROR(parser.AddArg<int>("integer").Default(5).Required(),
                         "Argument with a default value can't be required");
  }
  {
    Parser parser;
    ASSERT_RUNTIME_ERROR(
        parser.AddArg<int>("integer").Default(5).Options({1, 2}),
        "The contained argument value is not among valid options");
  }
  {
    Parser parser;
    ASSERT_RUNTIME_ERROR(
        parser.AddArg<int>("integer").Options({1, 2}).Default(5),
        "Value provided for an argument is not among valid options");
  }
  {
    Parser parser;
    ASSERT_RUNTIME_ERROR(
        parser.AddMultiArg<int>("integer").Required().Default({5}),
        "Required argument can't have a default value");
  }
  {
    Parser parser;
    ASSERT_RUNTIME_ERROR(
        parser.AddMultiArg<int>("integer").Default({5}).Required(),
        "Argument with a default value can't be required");
  }
  {
    Parser parser;
    ASSERT_RUNTIME_ERROR(
        parser.AddMultiArg<int>("integer").Default({5}).Options({1, 2}),
        "One of the contained values provided for an argument is not among "
        "valid options");
  }
  {
    Parser parser;
    ASSERT_RUNTIME_ERROR(
        parser.AddMultiArg<int>("integer").Options({1, 2}).Default({5}),
        "One of the values provided for an argument is not among valid "
        "options");
  }
}

TEST(Parser, CustomType) {
  {
    Parser parser;
    auto integers =
        parser.AddArg<IntPair>("integers").CastUsing(IntPairFromString);
    parser.ParseArgs({"binary", "--integers", "1,2"});
    EXPECT_EQ(integers->x, 1);
    EXPECT_EQ(integers->y, 2);
  }
  {
    Parser parser;
    ASSERT_RUNTIME_ERROR(parser.AddArg<IntPair>("integers").Options({{0, 1}}),
                         "No operator== defined for the type of the argument");
  }
  {
    Parser parser;
    ASSERT_RUNTIME_ERROR(
        parser.AddMultiArg<IntPair>("integers").Options({{0, 1}}),
        "No operator== defined for the type of the argument");
  }
  {
    Parser parser;
    parser.AddArg<IntPair>("integers");
    ASSERT_RUNTIME_ERROR(parser.ParseArgs({"binary", "--integers", "whatever"}),
                         "Value caster for an argument returned nothing");
  }
}

TEST(Parser, CustomParser) {
  auto SqrtFromStr = [](const std::string& str) {
    return std::sqrt(std::stod(str));
  };
  Parser parser;
  auto number = parser.AddArg<double>("number").CastUsing(SqrtFromStr);
  parser.ParseArgs({"binary", "--number", "64"});
  ASSERT_TRUE(number);
  EXPECT_EQ(*number, 8);
}

TEST(Parser, PositionalArgs) {
  Parser parser;
  parser.EnableFreeArgs();
  auto string = parser.AddPositionalArg<std::string>();
  auto integer = parser.AddPositionalArg<int>();
  parser.ParseArgs({"binary", "--number", "64", "free", "args", "go", "here"});
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
      {"binary", "--number", "64", "3.14", "free", "args", "go", "here"});
  ASSERT_TRUE(string);
  EXPECT_EQ(*string, "--number");
  ASSERT_TRUE(integer);
  EXPECT_EQ(*integer, 64);
  ASSERT_TRUE(number);
  EXPECT_THAT(*number, ::testing::DoubleEq(3.14));
  EXPECT_THAT(parser.FreeArgs(),
              ::testing::ElementsAre("free", "args", "go", "here"));
}

}  // namespace
}  // namespace argparse
