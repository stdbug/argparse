#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

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

#define ARGPARSE_FAIL(msg) ::argparse::detail::NotifyError(msg)

#define ARGPARSE_FAIL_IF(condition, msg) \
  if (condition) {                       \
    ARGPARSE_FAIL(msg);                  \
  }

struct No {};

template <typename T>
No operator==(const T&, const T&);

template <typename T>
struct EqualExists {
  static constexpr bool value =
      !std::is_same_v<decltype(*(T*)(0) == *(T*)(0)), No>;
};

}  // namespace detail

template <typename Dst>
std::optional<Dst> DefaultCaster(const std::string& str) {
  return std::nullopt;
}

template <>
std::optional<int> DefaultCaster(const std::string& str) {
  return std::stoi(str);
}

template <>
std::optional<double> DefaultCaster(const std::string& str) {
  return std::stod(str);
}

template <>
std::optional<std::string> DefaultCaster(const std::string& str) {
  return str;
}

template <typename T>
std::optional<bool> DefaultEquals(const T& a, const T& b) {
  if constexpr (detail::EqualExists<T>::value) {
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
    ARGPARSE_FAIL_IF(
        !value, "Value caster for argument `" + fullname() +
                    "` returned nothing. Did you provide a custom parser?");
    ARGPARSE_FAIL_IF(options_ && !IsValidValue(*value, *options_),
                     "Argument `" + str + "` provided for `" + fullname() +
                         "` casts to an illegal value");
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
        "Value provided for `" + fullname() + "` is not among valid options");
    value_ = std::move(value);
  }

  void set_options(std::vector<Type> options) {
    ARGPARSE_FAIL_IF(
        !detail::EqualExists<Type>::value,
        "No operator== defined for type of `" + fullname() + "` argument!");
    ARGPARSE_FAIL_IF(options.empty(), "Set of options can't be empty");
    ARGPARSE_FAIL_IF(value_ && !IsValidValue(*value_, options),
                     "Contained argument value provided for `" + fullname() +
                         "` is not among valid options");
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
                     "One of the values provided for `" + fullname() +
                         "` is not among valid options");
    values_ = std::move(values);
  }

  void set_options(std::vector<Type> options) {
    ARGPARSE_FAIL_IF(options.empty(), "Set of options can't be empty");
    ARGPARSE_FAIL_IF(
        !detail::EqualExists<Type>::value,
        "No operator== defined for type of `" + fullname() + "` argument!");
    ARGPARSE_FAIL_IF(std::any_of(values_.begin(), values_.end(),
                                 [&options](const Type& value) {
                                   return !IsValidValue(value, options);
                                 }),
                     "One of the contained values provided for `" + fullname() +
                         "` is not among valid options");
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

  ArgHolderWrapper& CastWith(std::function<Type(const std::string&)> caster) {
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

  MultiArgHolderWrapper& CastWith(
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

class Parser {
public:
  Parser(bool allow_free_args = false)
      : holders_()
      , short_long_mapping_()
      , free_args_(allow_free_args ? std::optional(std::vector<std::string>())
                                   : std::nullopt) {}

  FlagHolderWrapper AddFlag(const std::string& fullname, char shortname,
                            const std::string& help = "") {
    CheckOptionEntry(fullname, shortname);
    auto holder = std::make_unique<FlagHolder>(fullname, shortname, help);
    FlagHolderWrapper wrapper(holder.get());
    holders_[fullname] = std::move(holder);
    return wrapper;
  }

  FlagHolderWrapper AddFlag(const std::string& fullname,
                            const std::string& help = "") {
    return AddFlag(fullname, '\0', help);
  }

  template <typename Type>
  ArgHolderWrapper<Type> AddArg(const std::string& fullname, char shortname,
                                const std::string& help = "") {
    CheckOptionEntry(fullname, shortname);
    auto holder = std::make_unique<ArgHolder<Type>>(fullname, shortname, help);
    ArgHolderWrapper<Type> wrapper(holder.get());
    holders_[fullname] = std::move(holder);
    return wrapper;
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
    CheckOptionEntry(fullname, shortname);
    auto holder =
        std::make_unique<MultiArgHolder<Type>>(fullname, shortname, help);
    MultiArgHolderWrapper<Type> wrapper(holder.get());
    holders_[fullname] = std::move(holder);
    return wrapper;
  }

  template <typename Type>
  MultiArgHolderWrapper<Type> AddMultiArg(const std::string& fullname,
                                          const std::string& help = "") {
    return AddMultiArg<Type>(fullname, '\0', help);
  }

  void ParseArgs(const std::vector<std::string>& args) {
    for (size_t i = 1; i < args.size(); i++) {
      if (args[i].empty()) {
        continue;
      }

      ARGPARSE_FAIL_IF(args[i].length() < 2, "Unknown option: " + args[i]);

      if (args[i].substr(0, 2) == "--") {
        auto [name, value] = detail::SplitLongArg(args[i].substr(2));
        auto it = holders_.find(name);
        ARGPARSE_FAIL_IF(it == holders_.end(), "Unknown option: " + name);
        it->second->OnFlag();
        if (!it->second->AcceptsValue()) {
          ARGPARSE_FAIL_IF(value.has_value(),
                           "Option `" + name + "` doesn't accept values");
          continue;
        }
        if (!value) {
          ARGPARSE_FAIL_IF(
              i + 1 >= args.size(),
              "Now value provided for option: " + it->second->fullname());
          i++;
          value = args[i];
        }
        it->second->OnValue(*value);
        continue;
      }

      if (args[i][0] == '-') {
        for (size_t j = 1; j < args[i].size(); j++) {
          char ch = args[i][j];
          auto it = short_long_mapping_.find(ch);
          ARGPARSE_FAIL_IF(it == short_long_mapping_.end(),
                           std::string("Unknown short option: `") + ch + "`");
          auto ihld = holders_.find(it->second);
          ARGPARSE_FAIL_IF(ihld == holders_.end(),
                           std::string("Unknown short option: `") + ch + "`");
          ihld->second->OnFlag();
          if (!ihld->second->AcceptsValue()) {
            continue;
          }
          ARGPARSE_FAIL_IF(j != args[i].size() - 1,
                           std::string("Short option with argument (`") + ch +
                               "`) must be the last one in it's group");
          ARGPARSE_FAIL_IF(
              i + 1 >= args.size(),
              std::string("Now value provided for short option `") + ch + "`");
          i++;
          ihld->second->OnValue(args[i]);
          break;
        }
        continue;
      }

      ARGPARSE_FAIL_IF(!free_args_, "Free arguments are not allowed");

      free_args_->push_back(args[i]);
    }

    CheckArgs();
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
  void CheckOptionEntry(const std::string& fullname, char shortname) {
    ARGPARSE_FAIL_IF(fullname == "help", "`help` is a predefined option");
    ARGPARSE_FAIL_IF(holders_.count(fullname),
                     "Argument `" + fullname + "` is already defined");
    ARGPARSE_FAIL_IF(short_long_mapping_.count(shortname),
                     std::string("Argument with shortname `") + shortname +
                         "` is already defined");
    if (shortname != '\0') {
      short_long_mapping_[shortname] = fullname;
    }
  }

  void CheckArgs() const {
    for (auto& [name, arg] : holders_) {
      ARGPARSE_FAIL_IF(arg->required() && !arg->HasValue(),
                       "Now value provided for option: " + name);
    }
  }

  std::unordered_map<std::string, std::unique_ptr<ArgHolderBase>> holders_;
  std::unordered_map<char, std::string> short_long_mapping_;
  std::optional<std::vector<std::string>> free_args_;
};

}  // namespace argparse
