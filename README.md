# argparse: single-header C++ command line argument parser library
## Basic usage
Include the header
```cpp
#include <argparse.h>
```

Declare the parser
```cpp
argparse::Parser parser;
```

Set arguments that may appear in the command line. Each `Add*` method will
return a holder that can be used to access argument values after they are parsed
```cpp
// flag, set by mentioning it among command line arguments
// ("--flag" or "-f")
// can be mentioned multiple times (e.g. "-fff")
auto flag = parser.AddFlag("flag", 'f', "Optional help string");

// integer value passed by one of these ways:
// --integer 42
// --integer=42
// -i 42
// -i42
auto integer = parser.AddArg<int>("integer", 'i', "Some integer");

// argument that may be passed multiple times, e.g.
// --double 3.14 --double 2.71 --double 1 --double=0,1,2
auto doubles = parser.AddMultiArg<double>("double", 'd');

// string as-is, but without a short option
auto string = parser.AddArg<std::string>("string");
```

Parse arguments
```cpp
parser.ParseArgs(argc, argv);
```

Access parsed arguments via `operator*`

No check is required for flags as they only indicate the presence of the flag
marker in the command line
```cpp
if (*flag) {
  std::cout << "flag was set " << *flag << " times\n";
} else {
  std::cout << "flag wasn't set\n";
}
```

For non-flag arguments first check if it was set (unless a default value is set
or the argument is marked as required, see
[below](#default-and-required-values))
```cpp
if (integer) {
  std::cout << "integer was set equal to " << *integer << "\n";
} else {
  std::cout << "integer wasn't set\n";
}
```

Access individual multiarg entries using range-based for loop or `operator->`
```cpp
std::cout << "doubles: "
for (double x : *doubles) {
  std::cout << x << "\n";
}
std::cout << "\n"

std::cout << "doubles were set " << doubles->size() << " times:\n";
for (size_t i = 0; i < doubles->size(); i++) {
  std::cout << "doubles[" << i << "] = " << doubles->at(i) << "\n";
}
```

## Default and required values
Non-flag arguments can be marked as required or have a default value
```cpp
auto integer_with_default = parser.AddArg<int>("integer").Default(42);
auto required_double = parser.AddArg<double>("double").Required();
```

In both of these cases there is no need to check for argument presence. Any
argument with a default value will hold it even when it's not mentioned among
`argv`. If no value is provided for a required argument, then `ParseArgs` will
throw an exception
```cpp
std::cout << "integer = " << *integer_with_default << "\n";
std::cout << "double = " << *required_double << "\n";
```

## Acceptable options for arguments
A set of valid values can be provided for arguments
```cpp
parser.AddArg<int>("integer").Options({0, 42, 256});
```

### Misusage
Make sure that provided options are compatible. Examples of incompatible
options that will result in an exception:
```cpp
// argument with a default value can't be required
// (why would you set a default value then?)
parser.AddArg<int>("integer").Default(42).Required();

// same
parser.AddArg<int>("integer").Required().Default(42);
```

## Custom types
By default, `argparse::Parser` supports parsing built-in numeric types (`float`,
`double`, `long double`, `short`, `int`, `long`, `long long` and their unsigned
versions), `bool` and `std::string`. To be able to parse other types one can use
C++ operators or define a specialization of `argparse::TypeTraits` template:
```cpp

std::istream& operator>>(std::istream& stream, MyType& variable) {
  // implementation
}

// Required only when using Options or Default
std::ostream& operator<<(std::ostream& stream, MyType& variable) {
  // implementation
}

// Required only when using Options
bool operator==(const MyType& variable1, const MyType& variable2) {
  // implementation
}

namespace argparse {

// TypeTraits specialization will have higher priority over >> and == operators
template <>
class TypeTraits<MyType> {
public:
  static MyType FromString(const std::string& str) {
    // implementation
  }

  // Required only when using Options or Default
  static std::string ToString(const MyType& my_var) {
    // implementation
  }

  // Required only when using Options
  static bool Equal(const MyType& variable1, const MyType& variable2) {
    // implementation
  }
};

}  // namespace argparse


int main(int argc, char* argv[]) {
  argparse::Parser parser;
  auto my_var = parser.AddArg<MyType>("my-var").Default(MyType()).Options({MyType(1), MyType(2)});
  parser.Parse(argc, argv);
}
```

A program using `argparse::Parser` with custom types having neither required
operators nor TypeTraits specialization will fail to compile

## Global args
In order to create a global argument use `argparse::Add*` functions and
`ARGPARSE_DECLARE_GLOBAL_*` macros to declare arguments defined in external
compilation units
```
// library.cpp
auto integer = argparse::AddGlobalArg<int>("integer");

// main.cpp
ARGPARSE_DECLARE_GLOBAL_ARG(int, integer);

int main(int argc, char* argv[]) {
  argparse::Parser parser;
  auto local_flag = parser.AddFlag("flag");
  parser.ParseArgs(argc, argv);

  // use both `local_flag` and `integer` global argument
}
```

## Errors
All parsing errors will result in throwing `argparse::ArgparseError`. State of
the parser and holders (including globals) in this case is undefined.
Optionally, parser can be set to exit the program (via `exit` function)
```
parser.ExitOnFailure(exit_code, optional_usage_string);
```

# TODO
* `--help` option
