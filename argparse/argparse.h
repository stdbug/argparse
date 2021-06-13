#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#define ARGPARSE_FAIL(msg) ::argparse::detail::NotifyError(msg)

#define ARGPARSE_FAIL_IF(condition, msg) \
  if (condition) {                       \
    ARGPARSE_FAIL(msg);                  \
  }

#define ARGPARSE_ASSERT(condition) \
  ARGPARSE_FAIL_IF(!(condition), "Argparse internal assumptions failed")

#define ARGPARSE_RETURN_IF_FAILED(operation) \
  {                                          \
    detail::Status status = operation;       \
    if (status.Failed()) {                   \
      return status;                         \
    }                                        \
  }

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

template <typename Dst>
std::optional<Dst> DefaultCaster(const std::string& str) {
  if constexpr (std::is_same_v<std::decay_t<Dst>, bool>) {
    if (str == "false") {
      return false;
    }
    if (str == "true") {
      return true;
    }
    return std::nullopt;
  }
  if constexpr (std::is_integral_v<Dst>) {
    char* endptr;
    Dst result;
    if constexpr (std::is_signed_v<Dst>) {
      result = static_cast<Dst>(std::strtoull(str.c_str(), &endptr, 10));
    } else {
      result = static_cast<Dst>(std::strtoll(str.c_str(), &endptr, 10));
    }
    if (endptr != str.c_str() + str.length()) {
      return std::nullopt;
    }
    return result;
  }
  if constexpr (std::is_floating_point_v<Dst>) {
    char* endptr;
    Dst result = static_cast<Dst>(std::strtold(str.c_str(), &endptr));
    if (endptr != str.c_str() + str.length()) {
      return std::nullopt;
    }
    return result;
  }
  if constexpr (std::is_constructible_v<Dst, std::string>) {
    return str;
  }
  return std::nullopt;
}

template <typename T>
std::optional<bool> DefaultEquals(const T& a, const T& b) {
  if constexpr (meta::EqualExists<T>::value) {
    return a == b;
  } else {
    return std::nullopt;
  }
}

template <typename Type>
bool IsValidValue(const Type& value, const std::vector<Type>& options) {
  return std::any_of(options.begin(), options.end(),
                     [&value](const Type& option) {
                       return *DefaultEquals(value, option);
                     });
}

}  // namespace detail

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

    std::optional<Type> value = caster_(value_str);
    ARGPARSE_FAIL_IF(!value, "Failed to cast argument string to value type (`" +
                                 fullname() + "`)");
    ARGPARSE_FAIL_IF(options_ && !detail::IsValidValue(*value, *options_),
                     "Provided argument string casts to an illegal value (`" +
                         fullname() + "`)");

    value_ = std::move(*value);
  }

  void set_caster(
      std::function<std::optional<Type>(const std::string&)> caster) {
    caster_ = caster;
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
    ARGPARSE_FAIL_IF(options.empty(),
                     "Set of options can't be empty (`" + fullname() + "`)");
    ARGPARSE_FAIL_IF(!meta::EqualExists<Type>::value,
                     "No operator== defined for the type of the argument (`" +
                         fullname() + "`)");
    options_ = std::move(options);
  }

private:
  std::optional<Type> value_;
  std::optional<std::vector<Type>> options_;
  bool contains_default_ = true;
  std::function<std::optional<Type>(const std::string&)> caster_ =
      detail::DefaultCaster<Type>;
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

    std::optional<Type> value = caster_(value_str);
    ARGPARSE_FAIL_IF(!value, "Failed to cast string to value type");
    ARGPARSE_FAIL_IF(options_ && !detail::IsValidValue(*value, *options_),
                     "Provided argument string casts to an illegal value");

    values_.push_back(std::move(*value));
  }

  void set_caster(
      std::function<std::optional<Type>(const std::string&)> caster) {
    caster_ = caster;
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
    ARGPARSE_FAIL_IF(options.empty(),
                     "Set of options can't be empty (`" + fullname() + "`)");
    ARGPARSE_FAIL_IF(!meta::EqualExists<Type>::value,
                     "No operator== defined for the type of the argument (`" +
                         fullname() + "`)");
    options_ = std::move(options);
  }

private:
  std::vector<Type> values_;
  std::optional<std::vector<Type>> options_;
  bool contains_default_ = true;
  std::function<std::optional<Type>(const std::string&)> caster_ =
      detail::DefaultCaster<Type>;
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

  ArgHolderWrapper& CastUsing(std::function<Type(const std::string&)> caster) {
    ptr_->set_caster([caster](const std::string& str) -> std::optional<Type> {
      return caster(str);
    });
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

  MultiArgHolderWrapper& CastUsing(
      std::function<Type(const std::string&)> caster) {
    ptr_->set_caster([caster](const std::string& str) -> std::optional<Type> {
      return caster(str);
    });
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
                       "Now value provided for option `" + name + "`");
    }
  }

  size_t Size() const {
    return holders_.size();
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
      , free_args_(std::nullopt)
      , parse_global_args_(true) {}

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
  ArgHolderWrapper<Type> AddPositionalArg(const std::string& help = "") {
    return positionals_.AddArg<Type>(
        detail::PositionalArgumentName(positionals_.Size()), '\0', help);
  }

  template <typename... Types>
  std::tuple<ArgHolderWrapper<Types>...> AddPositionalArgs(
      const std::string& help = "") {
    return std::tuple<ArgHolderWrapper<Types>...>{
        AddPositionalArg<Types>(help)...};
  }

  void EnableFreeArgs() {
    free_args_ = std::vector<std::string>();
  }

  void IgnoreGlobalFlags() {
    parse_global_args_ = false;
  }

  void ParseArgs(const std::vector<std::string>& args,
                 const std::optional<std::string>& tail_mark = std::nullopt) {
    size_t positional_arg_count = 0;
    for (size_t i = 1, step; i < args.size(); i += step) {
      if (args[i] == tail_mark) {
        std::copy(args.begin() + i + 1, args.end(),
                  std::back_inserter(tail_args_));
        break;
      }

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
        ArgHolderBase* holder = positionals_.GetHolderByFullName(
            detail::PositionalArgumentName(positional_arg_count++));
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

  void ParseArgs(int argc, char* argv[],
                 const std::optional<std::string>& tail_mark = std::nullopt) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; i++) {
      args.push_back(argv[i]);
    }

    ParseArgs(args, tail_mark);
  }

  const std::vector<std::string>& FreeArgs() const {
    return *free_args_;
  }

  const std::vector<std::string>& TailArgs() const {
    return tail_args_;
  }

private:
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

  detail::Holders holders_;
  detail::Holders positionals_;
  std::optional<std::vector<std::string>> free_args_;
  std::vector<std::string> tail_args_;
  bool parse_global_args_;
};

}  // namespace argparse

#define ARGPARSE_DECLARE_GLOBAL_FLAG(name) \
  extern ::argparse::FlagHolderWrapper name;
#define ARGPARSE_DECLARE_GLOBAL_ARG(Type, name) \
  extern ::argparse::ArgHolderWrapper<Type> name;
#define ARGPARSE_DECLARE_GLOBAL_MULTIARG(Type, name) \
  extern ::argparse::MultiArgHolderWrapper<Type> name;
