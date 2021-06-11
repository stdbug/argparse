#pragma once

#include <algorithm>
#include <functional>
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

namespace argparse {
namespace detail {

inline std::tuple<std::string, std::optional<std::string>> SplitLongArg(
    const std::string& arg) {
  auto pos = arg.find("=");
  if (pos == std::string::npos) {
    return {arg, std::nullopt};
  }
  return {arg.substr(0, pos), arg.substr(pos + 1)};
}

inline void NotifyError(const std::string& msg) {
  throw std::runtime_error(msg);
}

}  // namespace detail

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

template <typename Dst>
std::optional<Dst> DefaultCaster(const std::string& str) {
  if constexpr (std::is_integral_v<Dst>) {
    if constexpr (std::is_signed_v<Dst>) {
      return static_cast<Dst>(std::stoll(str));
    } else {
      return static_cast<Dst>(std::stoul(str));
    }
  }
  if constexpr (std::is_floating_point_v<Dst>) {
    return static_cast<Dst>(std::stold(str));
  }
  if constexpr (std::is_same_v<Dst, std::string>) {
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
  virtual bool AcceptsValue() const = 0;
  virtual void OnFlag() = 0;
  virtual void OnValue(const std::string& value) = 0;

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
      , value_(false) {}

  virtual bool HasValue() const override {
    return true;
  }

  virtual bool AcceptsValue() const override {
    return false;
  }

  virtual void OnFlag() override {
    value_ = true;
  }

  virtual void OnValue(const std::string& str) override {
    (void)str;
    ARGPARSE_FAIL("Flag argument doesn't accept any value");
  }

  bool value() const {
    return value_;
  }

private:
  bool value_ = false;
};

template <typename Type>
class ArgHolder : public ArgHolderBase {
public:
  ArgHolder(std::string fullname, char shortname, std::string help)
      : ArgHolderBase(fullname, shortname, help) {}

  virtual bool HasValue() const override {
    return value_.has_value();
  }

  virtual bool AcceptsValue() const override {
    return true;
  }

  virtual void OnFlag() override {
    if (contains_default_) {
      value_.reset();
      contains_default_ = false;
    }
    ARGPARSE_FAIL_IF(HasValue(), "This argument accepts only one value");
  }

  virtual void OnValue(const std::string& str) override {
    ARGPARSE_FAIL_IF(HasValue(), "This argument accepts only one value");
    std::optional<Type> value = caster_(str);
    ARGPARSE_FAIL_IF(!value,
                     "Value caster for an argument returned nothing (`" +
                         fullname() + "`). Did you provide a custom parser?");
    ARGPARSE_FAIL_IF(options_ && !IsValidValue(*value, *options_),
                     "Provided argument casts to an illegal value (`" + str +
                         "` for argument `" + fullname() + "`)");
    value_ = std::move(value);
  }

  void set_caster(
      std::function<std::optional<Type>(const std::string&)> caster) {
    caster_ = caster;
  }

  const Type& value() const {
    return *value_;
  }

  void set_value(Type value) {
    ARGPARSE_FAIL_IF(
        options_ && !IsValidValue(value, *options_),
        "Value provided for an argument is not among valid options (`" +
            fullname() + "`)");
    value_ = std::move(value);
  }

  void set_options(std::vector<Type> options) {
    ARGPARSE_FAIL_IF(!meta::EqualExists<Type>::value,
                     "No operator== defined for the type of the argument (`" +
                         fullname() + "`)");
    ARGPARSE_FAIL_IF(options.empty(), "Set of options can't be empty");
    ARGPARSE_FAIL_IF(
        value_ && !IsValidValue(*value_, options),
        "The contained argument value is not among valid options (`" +
            fullname() + "`)");
    options_ = std::move(options);
  }

private:
  std::optional<Type> value_;
  std::optional<std::vector<Type>> options_;
  bool contains_default_ = true;
  std::function<std::optional<Type>(const std::string&)> caster_ =
      DefaultCaster<Type>;
};

template <typename Type>
class MultiArgHolder : public ArgHolderBase {
public:
  MultiArgHolder(std::string fullname, char shortname, std::string help)
      : ArgHolderBase(fullname, shortname, help) {}

  virtual bool HasValue() const override {
    return !values_.empty();
  }

  virtual bool AcceptsValue() const override {
    return true;
  }

  virtual void OnFlag() override {
    if (contains_default_) {
      values_.clear();
      contains_default_ = false;
    }
  }

  virtual void OnValue(const std::string& str) override {
    std::optional<Type> value = caster_(str);
    ARGPARSE_FAIL_IF(!value, "Value caster for argument `" + fullname() +
                                 "` returned nothing. Did you define "
                                 "`DefaultCaster` or provide custom parser?");
    ARGPARSE_FAIL_IF(options_ && !IsValidValue(*value, *options_),
                     "Argument `" + str + "` provided for `" + fullname() +
                         "` casts to an illegal value");
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
    ARGPARSE_FAIL_IF(options_ && std::any_of(values.begin(), values.end(),
                                             [this](const Type& value) {
                                               return !IsValidValue(value,
                                                                    *options_);
                                             }),
                     "One of the values provided for an argument is not among "
                     "valid options (`" +
                         fullname() + "`)");
    values_ = std::move(values);
  }

  void set_options(std::vector<Type> options) {
    ARGPARSE_FAIL_IF(options.empty(), "Set of options can't be empty");
    ARGPARSE_FAIL_IF(!meta::EqualExists<Type>::value,
                     "No operator== defined for the type of the argument (`" +
                         fullname() + "`)");
    ARGPARSE_FAIL_IF(std::any_of(values_.begin(), values_.end(),
                                 [&options](const Type& value) {
                                   return !IsValidValue(value, options);
                                 }),
                     "One of the contained values provided for an argument is "
                     "not among valid options (`" +
                         fullname() + "`)");
    options_ = std::move(options);
  }

private:
  std::vector<Type> values_;
  std::optional<std::vector<Type>> options_;
  bool contains_default_ = true;
  std::function<std::optional<Type>(const std::string&)> caster_ =
      DefaultCaster<Type>;
};

class FlagHolderWrapper {
public:
  FlagHolderWrapper(FlagHolder* ptr)
      : ptr_(ptr) {}

  bool operator*() const {
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
    ARGPARSE_FAIL_IF(ptr_->HasValue(),
                     "Argument with a default value can't be required");
    ptr_->set_required(true);
    return *this;
  }

  ArgHolderWrapper& Default(Type value) {
    ARGPARSE_FAIL_IF(ptr_->required(),
                     "Required argument can't have a default value");
    ptr_->set_value(value);
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

  bool HasValue() const {
    return ptr_->HasValue();
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
    ARGPARSE_FAIL_IF(ptr_->HasValue(),
                     "Argument with a default value can't be required");
    ptr_->set_required(true);
    return *this;
  }

  MultiArgHolderWrapper& Default(std::vector<Type> values) {
    ARGPARSE_FAIL_IF(ptr_->required(),
                     "Required argument can't have a default value");
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

  void CheckArgs() const {
    for (auto& [name, arg] : holders_) {
      ARGPARSE_FAIL_IF(arg->required() && !arg->HasValue(),
                       "Now value provided for option: " + name);
    }
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

  size_t Size() const {
    return holders_.size();
  }

private:
  void UpdateShortLongMapping(const std::string& fullname, char shortname) {
    if (shortname != '\0') {
      short_long_mapping_[shortname] = fullname;
    }
  }

private:
  std::unordered_map<std::string, std::unique_ptr<ArgHolderBase>> holders_;
  std::unordered_map<char, std::string> short_long_mapping_;
};

inline Holders* GlobalHolders() {
  static Holders* holders = new Holders;
  return holders;
}

inline std::string PositionalArgumentName(size_t position) {
  return "__positional_arg__" + std::to_string(position);
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
    return positinals_.AddArg<Type>(
        detail::PositionalArgumentName(positinals_.Size()), '\0', help);
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

  void ParseArgs(const std::vector<std::string>& args) {
    size_t positional_arg_count = 0;
    for (size_t i = 1, step; i < args.size(); i += step) {
      step = ParseLongArg(args, i);
      if (step > 0) {
        continue;
      }

      step = ParseShortArgs(args, i);
      if (step > 0) {
        continue;
      }

      if (positional_arg_count < positinals_.Size()) {
        ArgHolderBase* holder = positinals_.GetHolderByFullName(
            detail::PositionalArgumentName(positional_arg_count++));
        ARGPARSE_ASSERT(holder != nullptr);
        holder->OnValue(args[i]);
        step = 1;
        continue;
      }

      ARGPARSE_FAIL_IF(!free_args_, "Free arguments are not allowed");

      free_args_->push_back(args[i]);

      step = 1;
    }

    if (parse_global_args_) {
      detail::GlobalHolders()->CheckArgs();
    }

    holders_.CheckArgs();
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
  size_t ParseLongArg(const std::vector<std::string>& args, size_t offset) {
    if (args[offset].substr(0, 2) != "--") {
      return 0;
    }
    auto [name, value] = detail::SplitLongArg(args[offset].substr(2));
    ArgHolderBase* holder = GetHolderByFullName(name);
    if (holder == nullptr) {
      return 0;
    }
    holder->OnFlag();
    if (!holder->AcceptsValue()) {
      ARGPARSE_FAIL_IF(value.has_value(),
                       "Option `" + name + "` doesn't accept values");
      return 1;
    }

    if (value) {
      holder->OnValue(*value);
      return 1;
    }

    ARGPARSE_FAIL_IF(offset + 1 >= args.size(),
                     "No value provided for option: " + holder->fullname());
    holder->OnValue(args[offset + 1]);

    return 2;
  }

  size_t ParseShortArgs(const std::vector<std::string>& args, size_t offset) {
    const std::string& arg = args[offset];
    if (arg.front() != '-') {
      return 0;
    }

    // first, check if a group of short args is valid as whole before parsing
    for (size_t i = 1; i < arg.length(); i++) {
      char ch = arg[i];
      ArgHolderBase* holder = GetHolderByShortName(ch);
      if (holder == nullptr) {
        return 0;
      }
      if (holder->AcceptsValue() && i != arg.length() - 1) {
        // found a short arg accepting value which is not the last one in the
        // group - skipping
        return 0;
      }
    }

    // do the actual parsing
    for (size_t i = 1; i < arg.length(); i++) {
      char ch = arg[i];
      ArgHolderBase* holder = GetHolderByShortName(ch);
      ARGPARSE_ASSERT(holder != nullptr);
      holder->OnFlag();
      if (holder->AcceptsValue()) {
        ARGPARSE_ASSERT(i == arg.length() - 1);
        ARGPARSE_FAIL_IF(
            offset + 1 >= args.size(),
            std::string("Now value provided for short option `") + ch + "`");
        holder->OnValue(args[offset + 1]);
        return 2;
      }
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
  detail::Holders positinals_;
  std::optional<std::vector<std::string>> free_args_;
  bool parse_global_args_;
};

}  // namespace argparse

#define ARGPARSE_DECLARE_GLOBAL_FLAG(name) \
  extern ::argparse::FlagHolderWrapper name;
#define ARGPARSE_DECLARE_GLOBAL_ARG(Type, name) \
  extern ::argparse::ArgHolderWrapper<Type> name;
#define ARGPARSE_DECLARE_GLOBAL_MULTIARG(Type, name) \
  extern ::argparse::MultiArgHolderWrapper<Type> name;
