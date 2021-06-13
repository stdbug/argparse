#pragma once

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <optional>
#include <stdexcept>
#include <string>

#define ASSERT_ARGPARSE_ERROR(statement, msg)               \
  {                                                         \
    std::optional<std::string> what;                        \
    try {                                                   \
      statement;                                            \
    } catch (argparse::ArgparseError & err) {               \
      what = err.what();                                    \
    } catch (...) {}                                        \
    if (!what) {                                            \
      FAIL() << "No argparse::ArgparseError was thrown";    \
      GTEST_SKIP();                                         \
    }                                                       \
    if (what->find(msg) == std::string::npos) {             \
      FAIL() << std::string("No `") + msg +                 \
                    "` substring was found in the catched " \
                    "argparse::ArgparseError "              \
                    "message (`" +                          \
                    *what + "`)";                           \
      GTEST_SKIP();                                         \
    }                                                       \
    SUCCEED();                                              \
  }
