#include "argparse/argparse.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <iostream>

namespace argparse {

struct StreamReadable {
  int value;
};

std::istream& operator>>(std::istream& stream, StreamReadable& var) {
  return stream >> var.value;
}

struct StreamReadableAndCastable {
  int value;
};

std::istream& operator>>(std::istream&, StreamReadableAndCastable&) {
  throw ArgparseError("Shouldn't be called!");
}

template <>
class TypeTraits<StreamReadableAndCastable> {
public:
  static StreamReadableAndCastable FromString(const std::string& str) {
    return {TypeTraits<int>::FromString(str)};
  }
};

struct StreamReadableAndComparable {
  int value;
};

std::istream& operator>>(std::istream& stream,
                         StreamReadableAndComparable& var) {
  return stream >> var.value;
}

bool operator==(const StreamReadableAndComparable& a,
                const StreamReadableAndComparable& b) {
  return a.value == b.value;
}

struct Castable {
  int value;
};

template <>
class TypeTraits<Castable> {
public:
  static Castable FromString(const std::string& str) {
    return {TypeTraits<int>::FromString(str)};
  }
};

struct CastableWithOperatorEqual {
  int value;

  friend bool operator==(const CastableWithOperatorEqual& a,
                         const CastableWithOperatorEqual& b) {
    return a.value == b.value;
  }
};

template <>
class TypeTraits<CastableWithOperatorEqual> {
public:
  static CastableWithOperatorEqual FromString(const std::string& str) {
    return {TypeTraits<int>::FromString(str)};
  }
};

struct CastableWithTraitsEqual {
  int value;
};

template <>
class TypeTraits<CastableWithTraitsEqual> {
public:
  static CastableWithTraitsEqual FromString(const std::string& str) {
    return {TypeTraits<int>::FromString(str)};
  }

  static bool Equal(const CastableWithTraitsEqual& a,
                    const CastableWithTraitsEqual& b) {
    return a.value == b.value;
  }
};

struct CastableWithOperatorAndTraitsEqual {
  int value;
  friend bool operator==(const CastableWithOperatorAndTraitsEqual&,
                         const CastableWithOperatorAndTraitsEqual&) {
    throw ArgparseError("Shouldn't be called!");
  }
};

template <>
class TypeTraits<CastableWithOperatorAndTraitsEqual> {
public:
  static CastableWithOperatorAndTraitsEqual FromString(const std::string& str) {
    return {TypeTraits<int>::FromString(str)};
  }

  static bool Equal(const CastableWithOperatorAndTraitsEqual& a,
                    const CastableWithOperatorAndTraitsEqual& b) {
    return a.value == b.value;
  }
};

namespace {

TEST(CustomTypes, StreamReadable) {
  Parser parser;
  auto var = parser.AddArg<StreamReadable>("var");
  parser.ParseArgs({"binary", "--var", "1"});
  EXPECT_EQ(var->value, 1);
}

TEST(CustomTypes, StreamReadableAndCastable) {
  Parser parser;
  auto var = parser.AddArg<StreamReadableAndCastable>("var");
  parser.ParseArgs({"binary", "--var", "1"});
  EXPECT_EQ(var->value, 1);
}

TEST(CustomTypes, StreamReadableAndComparable) {
  Parser parser;
  auto var = parser.AddArg<StreamReadableAndComparable>("var").Options({{1}});
  parser.ParseArgs({"binary", "--var", "1"});
  EXPECT_EQ(var->value, 1);
}

TEST(CustomTypes, Castable) {
  Parser parser;
  auto var = parser.AddArg<Castable>("var");
  parser.ParseArgs({"binary", "--var", "1"});
  EXPECT_EQ(var->value, 1);
}

TEST(CustomTypes, CastableWithOperatorEqual) {
  Parser parser;
  auto var = parser.AddArg<CastableWithOperatorEqual>("var").Options({{1}});
  parser.ParseArgs({"binary", "--var", "1"});
  EXPECT_EQ(var->value, 1);
}

TEST(CustomTypes, CastableWithTraitsEqual) {
  Parser parser;
  auto var = parser.AddArg<CastableWithTraitsEqual>("var").Options({{1}});
  parser.ParseArgs({"binary", "--var", "1"});
  EXPECT_EQ(var->value, 1);
}

TEST(CustomTypes, CastableWithOperatorAndTraitsEqual) {
  Parser parser;
  auto var =
      parser.AddArg<CastableWithOperatorAndTraitsEqual>("var").Options({{1}});
  parser.ParseArgs({"binary", "--var", "1"});
  EXPECT_EQ(var->value, 1);
}

}  // namespace
}  // namespace argparse
