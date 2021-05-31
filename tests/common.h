#pragma once

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <optional>
#include <stdexcept>
#include <string>

#define ASSERT_RUNTIME_ERROR(statement, msg)                                   \
  {                                                                            \
    std::optional<std::string> what;                                           \
    try {                                                                      \
      statement;                                                               \
    } catch (std::runtime_error & err) { what = err.what(); } catch (...) {    \
    }                                                                          \
    if (!what) {                                                               \
      FAIL() << "No std::runtime_error was thrown";                            \
      GTEST_SKIP();                                                            \
    }                                                                          \
    if (what->find(msg) == std::string::npos) {                                \
      FAIL() << std::string("No `") + msg +                                    \
                    "` substring was found in the catched std::runtime_error " \
                    "message (`" +                                             \
                    *what + "`)";                                              \
      GTEST_SKIP();                                                            \
    }                                                                          \
    SUCCEED();                                                                 \
  }
