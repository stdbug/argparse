#include "argparse/argparse.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <iostream>

namespace argparse {

struct CustomTypeBase {
  int value;
};

struct StreamReadable : public CustomTypeBase {
  friend std::istream& operator>>(std::istream& stream, StreamReadable& var) {
    return stream >> var.value;
  }
};

struct StreamReadableAndCastable : public CustomTypeBase {
  friend std::istream& operator>>(std::istream&, StreamReadableAndCastable&) {
    throw ArgparseError("Shouldn't be called!");
  }
};

template <>
class TypeTraits<StreamReadableAndCastable> {
public:
  static StreamReadableAndCastable FromString(const std::string& str) {
    return {TypeTraits<int>::FromString(str)};
  }
};

struct StreamReadableAndOperatorComparable : public CustomTypeBase {
  friend std::istream& operator>>(std::istream& stream,
                                  StreamReadableAndOperatorComparable& var) {
    return stream >> var.value;
  }

  friend std::ostream& operator<<(
      std::ostream& stream, const StreamReadableAndOperatorComparable& var) {
    return stream << var.value;
  }

  friend bool operator==(const StreamReadableAndOperatorComparable& a,
                         const StreamReadableAndOperatorComparable& b) {
    return a.value == b.value;
  }
};

struct Castable : public CustomTypeBase {};

template <>
class TypeTraits<Castable> {
public:
  static Castable FromString(const std::string& str) {
    return {TypeTraits<int>::FromString(str)};
  }
};

struct CastableWithOperatorEqual : public CustomTypeBase {
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
  static std::string ToString(const CastableWithOperatorEqual& value) {
    return {TypeTraits<int>::ToString(value.value)};
  }
};

struct CastableWithTraitsEqual : public CustomTypeBase {};

template <>
class TypeTraits<CastableWithTraitsEqual> {
public:
  static CastableWithTraitsEqual FromString(const std::string& str) {
    return {TypeTraits<int>::FromString(str)};
  }

  static std::string ToString(const CastableWithTraitsEqual& value) {
    return {TypeTraits<int>::ToString(value.value)};
  }

  static bool Equal(const CastableWithTraitsEqual& a,
                    const CastableWithTraitsEqual& b) {
    return a.value == b.value;
  }
};

struct CastableWithOperatorEqualAndTraitsEqual : public CustomTypeBase {
  friend bool operator==(const CastableWithOperatorEqualAndTraitsEqual&,
                         const CastableWithOperatorEqualAndTraitsEqual&) {
    throw ArgparseError("Shouldn't be called!");
  }
};

template <>
class TypeTraits<CastableWithOperatorEqualAndTraitsEqual> {
public:
  static CastableWithOperatorEqualAndTraitsEqual FromString(
      const std::string& str) {
    return {TypeTraits<int>::FromString(str)};
  }

  static std::string ToString(
      const CastableWithOperatorEqualAndTraitsEqual& value) {
    return {TypeTraits<int>::ToString(value.value)};
  }

  static bool Equal(const CastableWithOperatorEqualAndTraitsEqual& a,
                    const CastableWithOperatorEqualAndTraitsEqual& b) {
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

TEST(CustomTypes, StreamReadableAndOperatorComparable) {
  Parser parser;
  auto var =
      parser.AddArg<StreamReadableAndOperatorComparable>("var").Options({{1}});
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

TEST(CustomTypes, CastableWithOperatorEqualAndTraitsEqual) {
  Parser parser;
  auto var =
      parser.AddArg<CastableWithOperatorEqualAndTraitsEqual>("var").Options(
          {{1}});
  parser.ParseArgs({"binary", "--var", "1"});
  EXPECT_EQ(var->value, 1);
}

}  // namespace
}  // namespace argparse
