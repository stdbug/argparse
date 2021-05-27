#include "argparse/argparse.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <tuple>

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

  ASSERT_TRUE(int1.HasValue());
  EXPECT_EQ(*int1, 42);
  ASSERT_TRUE(int2.HasValue());
  EXPECT_EQ(*int2, -2147483648);
  ASSERT_TRUE(int3.HasValue());
  EXPECT_FALSE(int4.HasValue());
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
    parser.AddFlag("flag", 'a');
    parser.AddArg<int>("int", 'b');

    EXPECT_ANY_THROW(parser.ParseArgs({"binary", "-ba", "42"}));
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
    EXPECT_ANY_THROW(parser.ParseArgs({"binary", "free_arg"}));
  }
  {
    Parser parser(true);
    ASSERT_NO_THROW(parser.ParseArgs({"binary", "free_arg"}));
    EXPECT_THAT(parser.FreeArgs(),
                ::testing::ElementsAre(std::string("free_arg")));
  }
  {
    Parser parser(true);
    auto integer = parser.AddArg<int>("integer");
    ASSERT_NO_THROW(parser.ParseArgs(
        std::vector<std::string>{"binary", "--integer", "5", "free_arg"}));
    EXPECT_THAT(parser.FreeArgs(),
                ::testing::ElementsAre(std::string("free_arg")));
    EXPECT_TRUE(integer.HasValue());
    EXPECT_EQ(*integer, 5);
  }
}

TEST(Parser, Options) {
  {
    Parser parser;
    parser.AddArg<int>("integer").Options({1, 2});
    EXPECT_ANY_THROW(parser.ParseArgs({"binary", "--integer", "5"}));
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
    EXPECT_ANY_THROW(parser.AddArg<int>("integer").Required().Default(5));
  }
  {
    Parser parser;
    EXPECT_ANY_THROW(parser.AddArg<int>("integer").Default(5).Required());
  }
  {
    Parser parser;
    EXPECT_ANY_THROW(parser.AddArg<int>("integer").Default(5).Options({1, 2}));
  }
  {
    Parser parser;
    EXPECT_ANY_THROW(parser.AddArg<int>("integer").Options({1, 2}).Default(5));
  }
  {
    Parser parser;
    EXPECT_ANY_THROW(
        parser.AddMultiArg<int>("integer").Required().Default({5}));
  }
  {
    Parser parser;
    EXPECT_ANY_THROW(
        parser.AddMultiArg<int>("integer").Default({5}).Required());
  }
  {
    Parser parser;
    EXPECT_ANY_THROW(
        parser.AddMultiArg<int>("integer").Default({5}).Options({1, 2}));
  }
  {
    Parser parser;
    EXPECT_ANY_THROW(
        parser.AddMultiArg<int>("integer").Options({1, 2}).Default({5}));
  }
}

TEST(Parser, CustomType) {
  {
    Parser parser;
    auto integers =
        parser.AddArg<IntPair>("integers").CastWith(IntPairFromString);
    parser.ParseArgs({"binary", "--integers", "1,2"});
    EXPECT_EQ(integers->x, 1);
    EXPECT_EQ(integers->y, 2);
  }
  {
    Parser parser;
    EXPECT_ANY_THROW(parser.AddArg<IntPair>("integers").Options({{0, 1}}));
  }
  {
    Parser parser;
    parser.AddArg<IntPair>("integers");
    EXPECT_ANY_THROW(parser.ParseArgs({"binary", "--integers", "whatever"}));
  }
}

TEST(Parser, CustomParser) {
  auto SqrtFromStr = [](const std::string& str) {
    return std::sqrt(std::stod(str));
  };
  Parser parser;
  auto number = parser.AddArg<double>("number").CastWith(SqrtFromStr);
  parser.ParseArgs({"binary", "--number", "64"});
  ASSERT_TRUE(number);
  EXPECT_EQ(*number, 8);
}

}  // namespace
}  // namespace argparse
