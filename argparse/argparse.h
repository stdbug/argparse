#pragma once

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <stdint.h>

#define ARGPARSE_FAIL(msg) ::argparse::detail::NotifyError(msg)

#define ARGPARSE_FAIL_IF(condition, msg) \
  if (condition) {                       \
    ARGPARSE_FAIL(msg);                  \
  }

#define ARGPARSE_ASSERT(condition) \
  ARGPARSE_FAIL_IF(!(condition), "Argparse internal assumptions failed")

namespace argparse {

class ArgparseError : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

namespace meta {

struct No {};

template <typename T>
No operator==(const T&, const T&);

template <typename T>
struct EqualExists {
  static constexpr bool value =
      !std::is_same_v<decltype(*(T*)(0) == *(T*)(0)), No>;
};

}  // namespace meta

namespace detail {

inline void NotifyError(const std::string& msg) {
  throw ::argparse::ArgparseError(msg);
}

inline std::tuple<std::string, std::optional<std::string>> SplitLongArg(
    const std::string& arg) {
  auto pos = arg.find("=");
  if (pos == std::string::npos) {
    return {arg, std::nullopt};
  }
  return {arg.substr(0, pos), arg.substr(pos + 1)};
}

inline std::string EscapeValue(std::string value) {
  if (!value.empty() && value[0] == '\\') {
    value.erase(0, 1);
  }

  return value;
}

template <typename T>
bool Equals(const T& a, const T& b) {
  if constexpr (meta::EqualExists<T>::value) {
    return a == b;
  }

  // should never get here
  ARGPARSE_ASSERT(false);
  return false;
}

template <typename Type>
bool IsValidValue(const Type& value, const std::vector<Type>& options) {
  return std::any_of(options.begin(), options.end(),
                     [&value](const Type& option) {
                       return Equals(value, option);
                     });
}

}  // namespace detail

template <typename Type>
Type Cast(const std::string& str);

template <>
inline bool Cast<bool>(const std::string& str) {
  if (str == "false") {
    return false;
  }
  if (str == "true") {
    return true;
  }
  ARGPARSE_FAIL("Failed to cast `" + str + "` to bool");
  return false;
}

template <>
inline long long int Cast<long long int>(const std::string& str) {
  char* endptr;
  long long int value = std::strtoll(str.c_str(), &endptr, 10);
  ARGPARSE_FAIL_IF(endptr != str.c_str() + str.length(),
                   "Failed to cast `" + str + "` to integer");
  return value;
}

template <>
inline unsigned long long int Cast<unsigned long long int>(
    const std::string& str) {
  char* endptr;
  unsigned long long int value = std::strtoull(str.c_str(), &endptr, 10);
  ARGPARSE_FAIL_IF(endptr != str.c_str() + str.length(),
                   "Failed to cast `" + str + "` to unsigned integer");
  return value;
}

template <>
inline long double Cast<long double>(const std::string& str) {
  char* endptr;
  long double value = std::strtold(str.c_str(), &endptr);
  ARGPARSE_FAIL_IF(endptr != str.c_str() + str.length(),
                   "Failed to cast `" + str + "` to floating point number");
  return value;
}

template <>
inline std::string Cast<std::string>(const std::string& str) {
  return str;
}

#define ARGPARSE_DEFINE_CONVERSION(Type, BaseType) \
  template <>                                      \
  inline Type Cast<Type>(const std::string& str) { \
    return static_cast<Type>(Cast<BaseType>(str)); \
  }

ARGPARSE_DEFINE_CONVERSION(long int, long long int)
ARGPARSE_DEFINE_CONVERSION(int, long long int)
ARGPARSE_DEFINE_CONVERSION(short int, long long int)
ARGPARSE_DEFINE_CONVERSION(unsigned long int, unsigned long long int)
ARGPARSE_DEFINE_CONVERSION(unsigned int, unsigned long long int)
ARGPARSE_DEFINE_CONVERSION(unsigned short int, unsigned long long int)
ARGPARSE_DEFINE_CONVERSION(double, long double)
ARGPARSE_DEFINE_CONVERSION(float, long double)

class ArgHolderBase {
public:
  ArgHolderBase(std::string fullname, char shortname, std::string help,
                bool required = false)
      : fullname_(std::move(fullname))
      , shortname_(shortname)
      , help_(std::move(help))
      , required_(required) {}

  virtual ~ArgHolderBase() = default;

  virtual bool HasValue() const = 0;
  virtual bool RequiresValue() const = 0;

  virtual void ProcessFlag() = 0;
  virtual void ProcessValue(const std::string& value_str) = 0;

  const std::string& fullname() const {
    return fullname_;
  }
  char shortname() const {
    return shortname_;
  }
  const std::string& help() const {
    return help_;
  }
  void set_required(bool required) {
    ARGPARSE_FAIL_IF(HasValue(),
                     "Argument with a default value can't be required");
    required_ = required;
  }
  bool required() const {
    return required_;
  }

private:
  std::string fullname_;
  char shortname_;
  std::string help_;
  bool required_;
};

class FlagHolder : public ArgHolderBase {
public:
  FlagHolder(std::string fullname, char shortname, std::string help)
      : ArgHolderBase(fullname, shortname, help)
      , value_(0) {}

  virtual bool HasValue() const override {
    return true;
  }

  virtual bool RequiresValue() const override {
    return false;
  }

  virtual void ProcessFlag() override {
    value_++;
  }

  virtual void ProcessValue(const std::string& value_str) override {
    (void)value_str;
    ARGPARSE_FAIL("Flags don't accept values");
  }

  size_t value() const {
    return value_;
  }

private:
  size_t value_;
};

template <typename Type>
class ArgHolder : public ArgHolderBase {
public:
  ArgHolder(std::string fullname, char shortname, std::string help)
      : ArgHolderBase(fullname, shortname, help) {}

  virtual bool HasValue() const override {
    return value_.has_value();
  }

  virtual bool RequiresValue() const override {
    return true;
  }

  virtual void ProcessFlag() override {
    ARGPARSE_FAIL("Argument requires a value (`" + fullname() + "`)");
  }

  virtual void ProcessValue(const std::string& value_str) override {
    ARGPARSE_FAIL_IF(HasValue() && !contains_default_,
                     "Argument accepts only one value (`" + fullname() + "`)");

    Type value = Cast<Type>(value_str);
    ARGPARSE_FAIL_IF(options_ && !detail::IsValidValue(value, *options_),
                     "Provided argument string casts to an illegal value (`" +
                         fullname() + "`)");

    value_ = std::move(value);
  }

  const Type& value() const {
    return *value_;
  }

  void set_value(Type value) {
    ARGPARSE_FAIL_IF(required(),
                     "Required argument can't have a default value");
    value_ = std::move(value);
  }

  void set_options(std::vector<Type> options) {
    static_assert(meta::EqualExists<Type>::value,
                  "No operator== defined for the type of the argument");
    ARGPARSE_FAIL_IF(options.empty(),
                     "Set of options can't be empty (`" + fullname() + "`)");
    options_ = std::move(options);
  }

private:
  std::optional<Type> value_;
  std::optional<std::vector<Type>> options_;
  bool contains_default_ = true;
};

template <typename Type>
class MultiArgHolder : public ArgHolderBase {
public:
  MultiArgHolder(std::string fullname, char shortname, std::string help)
      : ArgHolderBase(fullname, shortname, help) {}

  virtual bool HasValue() const override {
    return !values_.empty();
  }

  virtual bool RequiresValue() const override {
    return true;
  }

  virtual void ProcessFlag() override {
    ARGPARSE_FAIL("This argument requires a value");
  }

  virtual void ProcessValue(const std::string& value_str) override {
    if (contains_default_) {
      values_.clear();
      contains_default_ = false;
    }

    Type value = Cast<Type>(value_str);
    ARGPARSE_FAIL_IF(options_ && !detail::IsValidValue(value, *options_),
                     "Provided argument string casts to an illegal value");

    values_.push_back(std::move(value));
  }

  const std::vector<Type>& values() const {
    return values_;
  }

  void set_values(std::vector<Type> values) {
    ARGPARSE_FAIL_IF(required(),
                     "Required argument can't have a default value");
    values_ = std::move(values);
  }

  void set_options(std::vector<Type> options) {
    static_assert(meta::EqualExists<Type>::value,
                  "No operator== defined for the type of the argument");
    ARGPARSE_FAIL_IF(options.empty(),
                     "Set of options can't be empty (`" + fullname() + "`)");
    options_ = std::move(options);
  }

private:
  std::vector<Type> values_;
  std::optional<std::vector<Type>> options_;
  bool contains_default_ = true;
};

class FlagHolderWrapper {
public:
  FlagHolderWrapper(FlagHolder* ptr)
      : ptr_(ptr) {}

  size_t operator*() const {
    return ptr_->value();
  }

private:
  FlagHolder* ptr_;
};

template <typename Type>
class ArgHolderWrapper {
public:
  ArgHolderWrapper(ArgHolder<Type>* ptr)
      : ptr_(ptr) {}

  ArgHolderWrapper& Required() {
    ptr_->set_required(true);
    return *this;
  }

  ArgHolderWrapper& Default(Type value) {
    ptr_->set_value(std::move(value));
    return *this;
  }

  ArgHolderWrapper& Options(std::vector<Type> options) {
    ptr_->set_options(std::move(options));
    return *this;
  }

  explicit operator bool() const {
    return ptr_->HasValue();
  }

  const Type& operator*() const {
    return ptr_->value();
  }

  const Type* operator->() const {
    return &ptr_->value();
  }

private:
  ArgHolder<Type>* ptr_;
};

template <typename Type>
class MultiArgHolderWrapper {
public:
  MultiArgHolderWrapper(MultiArgHolder<Type>* ptr)
      : ptr_(ptr) {}

  MultiArgHolderWrapper& Required() {
    ptr_->set_required(true);
    return *this;
  }

  MultiArgHolderWrapper& Default(std::vector<Type> values) {
    ptr_->set_values(std::move(values));
    return *this;
  }

  MultiArgHolderWrapper& Options(std::vector<Type> options) {
    ptr_->set_options(std::move(options));
    return *this;
  }

  size_t Size() const {
    return ptr_->values().size();
  }

  bool Empty() const {
    return Size() == 0;
  }

  const Type& operator[](size_t index) const {
    return ptr_->values()[index];
  }

  const std::vector<Type>& Values() const {
    return ptr_->values();
  }

private:
  MultiArgHolder<Type>* ptr_;
};

namespace detail {

class Holders {
public:
  FlagHolderWrapper AddFlag(const std::string& fullname, char shortname,
                            const std::string& help = "") {
    CheckOptionEntry(fullname, shortname);
    UpdateShortLongMapping(fullname, shortname);
    auto holder = std::make_unique<FlagHolder>(fullname, shortname, help);
    FlagHolderWrapper wrapper(holder.get());
    holders_[fullname] = std::move(holder);
    return wrapper;
  }

  template <typename Type>
  ArgHolderWrapper<Type> AddArg(const std::string& fullname, char shortname,
                                const std::string& help = "") {
    CheckOptionEntry(fullname, shortname);
    UpdateShortLongMapping(fullname, shortname);
    auto holder = std::make_unique<ArgHolder<Type>>(fullname, shortname, help);
    ArgHolderWrapper<Type> wrapper(holder.get());
    holders_[fullname] = std::move(holder);
    return wrapper;
  }

  template <typename Type>
  MultiArgHolderWrapper<Type> AddMultiArg(const std::string& fullname,
                                          char shortname,
                                          const std::string& help = "") {
    CheckOptionEntry(fullname, shortname);
    UpdateShortLongMapping(fullname, shortname);
    auto holder =
        std::make_unique<MultiArgHolder<Type>>(fullname, shortname, help);
    MultiArgHolderWrapper<Type> wrapper(holder.get());
    holders_[fullname] = std::move(holder);
    return wrapper;
  }

  ArgHolderBase* GetHolderByFullName(const std::string& fullname) {
    auto it = holders_.find(fullname);
    if (it == holders_.end()) {
      return nullptr;
    }

    return it->second.get();
  }

  ArgHolderBase* GetHolderByShortName(char shortname) {
    auto im = short_long_mapping_.find(shortname);
    if (im == short_long_mapping_.end()) {
      return nullptr;
    }

    auto it = holders_.find(im->second);
    if (it == holders_.end()) {
      return nullptr;
    }

    return it->second.get();
  }

  void CheckOptionEntry(const std::string& fullname, char shortname) {
    ARGPARSE_FAIL_IF(fullname == "help", "`help` is a predefined option");
    ARGPARSE_FAIL_IF(holders_.count(fullname),
                     "Argument is already defined (`" + fullname + "`)");
    ARGPARSE_FAIL_IF(
        short_long_mapping_.count(shortname),
        std::string("Argument with shortname is already defined (`") +
            shortname + "`)");
  }

  void PerformPostParseCheck() const {
    for (auto& [name, arg] : holders_) {
      ARGPARSE_FAIL_IF(arg->required() && !arg->HasValue(),
                       "No value provided for option `" + name + "`");
    }
  }

  size_t Size() const {
    return holders_.size();
  }

  std::vector<std::string> OptionNames() const {
    std::vector<std::string> result;
    for (const auto& [name, _] : holders_) {
      result.push_back(name);
    }

    return result;
  }

private:
  void UpdateShortLongMapping(const std::string& fullname, char shortname) {
    ARGPARSE_ASSERT(!short_long_mapping_.count(shortname));
    if (shortname != '\0') {
      short_long_mapping_[shortname] = fullname;
    }
  }

  std::unordered_map<std::string, std::unique_ptr<ArgHolderBase>> holders_;
  std::unordered_map<char, std::string> short_long_mapping_;
};

inline Holders* GlobalHolders() {
  static Holders* holders = new Holders;
  return holders;
}

inline std::string PositionalArgumentName(size_t position) {
  return "__positional_argument__" + std::to_string(position);
}

}  // namespace detail

inline FlagHolderWrapper AddGlobalFlag(const std::string& fullname,
                                       char shortname,
                                       const std::string& help = "") {
  return detail::GlobalHolders()->AddFlag(fullname, shortname, help);
}

inline FlagHolderWrapper AddGlobalFlag(const std::string& fullname,
                                       const std::string& help = "") {
  return AddGlobalFlag(fullname, '\0', help);
}

template <typename Type>
ArgHolderWrapper<Type> AddGlobalArg(const std::string& fullname, char shortname,
                                    const std::string& help = "") {
  return detail::GlobalHolders()->AddArg<Type>(fullname, shortname, help);
}

template <typename Type>
ArgHolderWrapper<Type> AddGlobalArg(const std::string& fullname,
                                    const std::string& help = "") {
  return AddGlobalArg<Type>(fullname, '\0', help);
}

template <typename Type>
MultiArgHolderWrapper<Type> AddGlobalMultiArg(const std::string& fullname,
                                              char shortname,
                                              const std::string& help = "") {
  return detail::GlobalHolders()->AddMultiArg<Type>(fullname, shortname, help);
}

template <typename Type>
MultiArgHolderWrapper<Type> AddGlobalMultiArg(const std::string& fullname,
                                              const std::string& help = "") {
  return AddGlobalMultiArg<Type>(fullname, '\0', help);
}

class Parser {
public:
  Parser()
      : holders_()
      , positionals_()
      , free_args_(std::nullopt)
      , parse_global_args_(true)
      , usage_string_(std::nullopt)
      , exit_code_(std::nullopt) {}

  FlagHolderWrapper AddFlag(const std::string& fullname, char shortname,
                            const std::string& help = "") {
    if (parse_global_args_) {
      detail::GlobalHolders()->CheckOptionEntry(fullname, shortname);
    }
    return holders_.AddFlag(fullname, shortname, help);
  }

  FlagHolderWrapper AddFlag(const std::string& fullname,
                            const std::string& help = "") {
    return AddFlag(fullname, '\0', help);
  }

  template <typename Type>
  ArgHolderWrapper<Type> AddArg(const std::string& fullname, char shortname,
                                const std::string& help = "") {
    if (parse_global_args_) {
      detail::GlobalHolders()->CheckOptionEntry(fullname, shortname);
    }
    return holders_.AddArg<Type>(fullname, shortname, help);
  }

  template <typename Type>
  ArgHolderWrapper<Type> AddArg(const std::string& fullname,
                                const std::string& help = "") {
    return AddArg<Type>(fullname, '\0', help);
  }

  template <typename Type>
  MultiArgHolderWrapper<Type> AddMultiArg(const std::string& fullname,
                                          char shortname,
                                          const std::string& help = "") {
    if (parse_global_args_) {
      detail::GlobalHolders()->CheckOptionEntry(fullname, shortname);
    }
    return holders_.AddMultiArg<Type>(fullname, shortname, help);
  }

  template <typename Type>
  MultiArgHolderWrapper<Type> AddMultiArg(const std::string& fullname,
                                          const std::string& help = "") {
    return AddMultiArg<Type>(fullname, '\0', help);
  }

  template <typename Type>
  ArgHolderWrapper<Type> AddPositionalArg() {
    return positionals_.AddArg<Type>(
        detail::PositionalArgumentName(positionals_.Size()), '\0');
  }

  template <typename... Types>
  std::tuple<ArgHolderWrapper<Types>...> AddPositionalArgs() {
    return std::tuple<ArgHolderWrapper<Types>...>{AddPositionalArg<Types>()...};
  }

  void EnableFreeArgs() {
    free_args_ = std::vector<std::string>();
  }

  void IgnoreGlobalFlags() {
    parse_global_args_ = false;
  }

  void ExitOnFailure(int exit_code) {
    exit_code_ = exit_code;
  }

  void SetUsageString(std::string usage_string) {
    usage_string_ = std::move(usage_string);
  }

  void ParseArgs(const std::vector<std::string>& args) {
    try {
      DoParseArgs(args);
    } catch (const ArgparseError& error) {
      if (!exit_code_) {
        throw;
      }
      if (usage_string_) {
        std::cerr << *usage_string_ << "\n";
      } else {
        std::cerr << "Failed to parse arguments. Error message: "
                  << error.what() << "\n\n";
        std::cerr << DefaultUsageString(args[0]) << "\n";
      }
      exit(*exit_code_);
    }
  }

  void ParseArgs(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; i++) {
      args.push_back(argv[i]);
    }

    ParseArgs(args);
  }

  const std::vector<std::string>& FreeArgs() const {
    return *free_args_;
  }

private:
  void DoParseArgs(const std::vector<std::string>& args) {
    size_t positional_arg_count = 0;
    for (size_t i = 1, step; i < args.size(); i += step) {
      if (args[i].length() > 2 && args[i].substr(0, 2) == "--") {
        step = ParseLongArg(args, i);
        ARGPARSE_ASSERT(step > 0);
        continue;
      }

      if (args[i].length() > 1 && args[i][0] == '-') {
        step = ParseShortArgs(args, i);
        ARGPARSE_ASSERT(step > 0);
        continue;
      }

      if (positional_arg_count < positionals_.Size()) {
        ArgHolderBase* holder = GetPositionalArgById(positional_arg_count++);
        ARGPARSE_ASSERT(holder != nullptr);
        holder->ProcessValue(detail::EscapeValue(args[i]));
        step = 1;
        continue;
      }

      ARGPARSE_FAIL_IF(!free_args_, "Free arguments are not enabled");

      free_args_->push_back(detail::EscapeValue(args[i]));

      step = 1;
    }

    PerformPostParseCheck();
  }

  void PerformPostParseCheck() const {
    if (parse_global_args_) {
      detail::GlobalHolders()->PerformPostParseCheck();
    }

    holders_.PerformPostParseCheck();

    positionals_.PerformPostParseCheck();
  }

  size_t ParseLongArg(const std::vector<std::string>& args, size_t offset) {
    ARGPARSE_ASSERT(args[offset].length() > 2 &&
                    args[offset].substr(0, 2) == "--");

    auto [name, value] = detail::SplitLongArg(args[offset].substr(2));
    ArgHolderBase* holder = GetHolderByFullName(name);
    ARGPARSE_FAIL_IF(holder == nullptr, "Unknown long option (`" + name + "`)");

    if (value) {
      ARGPARSE_FAIL_IF(!holder->RequiresValue(),
                       "Long option doesn't require a value (`" + name + "`)");

      holder->ProcessValue(*value);
      return 1;
    }

    if (!holder->RequiresValue()) {
      holder->ProcessFlag();
      return 1;
    }

    ARGPARSE_FAIL_IF(offset + 1 >= args.size(),
                     "No value provided for a long option (`" + name + "`)");

    holder->ProcessValue(args[offset + 1]);
    return 2;
  }

  size_t ParseShortArgs(const std::vector<std::string>& args, size_t offset) {
    ARGPARSE_ASSERT(args[offset].length() > 1 && args[offset][0] == '-');

    const std::string& arg = args[offset];

    for (size_t i = 1; i < arg.length(); i++) {
      char ch = arg[i];
      ArgHolderBase* holder = GetHolderByShortName(ch);
      ARGPARSE_FAIL_IF(holder == nullptr,
                       std::string("Unknown short option (`") + ch + "`)");

      if (holder->RequiresValue()) {
        if (i + 1 == arg.length()) {
          // last short option of a group requiring a value
          // (e.g. -euxo pipefail)
          ARGPARSE_FAIL_IF(
              offset + 1 >= args.size(),
              std::string("No value provided for a short option (`") + ch +
                  "`)");

          holder->ProcessValue(args[offset + 1]);
          return 2;
        }

        if (i == 1 && arg.length() > 2) {
          // option like -j5
          holder->ProcessValue(arg.substr(2));
          return 1;
        }

        ARGPARSE_FAIL(
            "Short option requiring an argument is not allowed in the middle "
            "of short options group");
      }

      holder->ProcessFlag();
    }

    return 1;
  }

  ArgHolderBase* GetPositionalArgById(size_t id) {
    return positionals_.GetHolderByFullName(detail::PositionalArgumentName(id));
  }

  ArgHolderBase* GetHolderByFullName(const std::string& fullname) {
    if (parse_global_args_) {
      auto holder = detail::GlobalHolders()->GetHolderByFullName(fullname);
      if (holder) {
        return holder;
      }
    }

    return holders_.GetHolderByFullName(fullname);
  }

  ArgHolderBase* GetHolderByShortName(char shortname) {
    if (parse_global_args_) {
      auto holder = detail::GlobalHolders()->GetHolderByShortName(shortname);
      if (holder) {
        return holder;
      }
    }

    return holders_.GetHolderByShortName(shortname);
  }

  static std::string OptionsDescription(detail::Holders& holders) {
    static const size_t kSecondColumnIndent = 24;
    std::string description;
    for (auto name : holders.OptionNames()) {
      ArgHolderBase* holder = holders.GetHolderByFullName(name);
      std::string line = "  ";
      if (holder->shortname() != '\0') {
        line += std::string("-") + holder->shortname() + ", ";
      } else {
        line += "    ";
      }
      line += "--" + holder->fullname() + "        ";
      for (size_t i = line.length(); i < kSecondColumnIndent; i++) {
        line.push_back(' ');
      }
      line += holder->help();
      if (holder->required()) {
        line += " (required)";
      }
      description += line + "\n";
    }

    return description;
  }

  std::string DefaultUsageString(const std::string& argv0) {
    std::string help_string = "Usage: " + argv0;
    if (positionals_.Size() > 0) {
      help_string += " POSITIONALS";
    }
    help_string += " OPTIONS";
    help_string += "\n";
    help_string += "\n";

    help_string += "Options:\n";
    if (parse_global_args_) {
      help_string += OptionsDescription(*detail::GlobalHolders());
    }

    help_string += OptionsDescription(holders_);

    return help_string;
  }

  detail::Holders holders_;
  detail::Holders positionals_;
  std::optional<std::vector<std::string>> free_args_;
  bool parse_global_args_;
  std::optional<std::string> usage_string_;
  std::optional<int> exit_code_;
};

}  // namespace argparse

#define ARGPARSE_DECLARE_GLOBAL_FLAG(name) \
  extern ::argparse::FlagHolderWrapper name;
#define ARGPARSE_DECLARE_GLOBAL_ARG(Type, name) \
  extern ::argparse::ArgHolderWrapper<Type> name;
#define ARGPARSE_DECLARE_GLOBAL_MULTIARG(Type, name) \
  extern ::argparse::MultiArgHolderWrapper<Type> name;
