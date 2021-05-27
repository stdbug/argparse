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

Set arguments that may appear in the command line. Each `Add*` method will return
a holder that can be used to access argument values after they are parsed
```cpp
// boolean flag, set by mentioning it among command line arguments
// ("--boolean" or "-b")
auto boolean = parser.AddFlag("boolean", 'b');

// integer value passed by one these ways:
// --integer 42
// --integer=42
// -i 42
auto integer = parser.AddArg<int>("integer", 'i');

// argument that may be mentioned multiple times, e.g.
// --double 3.14 --double 2.71 --double 1
auto doubles = parser.AddMultiArg<double>("double", 'd');

// string as-is, but without short option
auto string = parser.AddArg<std::string>("string");
```

Parse arguments
```cpp
parser.ParseArgs(argc, argv);
```

Access parsed arguments via `operator*`

No check is required for boolean flags as they only indicate the presence of
the flag marker in the command line
```cpp
if (*boolean) {
  std::cout << "boolean was set\n";
} else {
  std::cout << "boolean wasn't set\n";
}
```

For non-boolean flags first check if it was set (unless a default value is set
or the argument marked as required, see [below](#default-and-required-values))
```cpp
if (integer) {
  std::cout << "integer was set equal to " << *integer << "\n";
} else {
  std::cout << "integer wasn't set\n";
}
```

Access individual multiarg entries using `operator[]`
```cpp
std::cout << "doubles were set " << doubles.Size() << " times:\n";
for (size_t i = 0; i < doubles.Size(); i++) {
  std::cout << "doubles[" << i << "] = " << doubles[i] << "\n";
}
```

## Default and required values
Non-boolean argumens can be marked as required or have a default value
```cpp
auto integer_with_default = parser.AddArg<int>("integer").Default(42);
auto required_double = parser.AddArg<double>("double").Required();
```

In both of these cases there is no need to check for argument presence. Any
argument with a default value will hold it even when it's marker is not among
`argv`. If no value is provided for a required argument, then `ParseArgs` will
throw an exception.
```cpp
std::cout << "integer = " << *integer_with_default << "\n";
std::cout << "double = " << *required_double << "\n";
```

## Acceptable options for arguments
A set of valid values can be provided for arguments
```cpp
parser.AddArg<int>("integer").Options({0, 42, 256});
```

## Misusage
Make sure that provided options are compatible. Some examples of incompatible
options that will result in an exception:
```cpp
// argument with a default value can't be required
// (why would you set a default value then?)
parser.AddArg<int>("integer").Default(42).Required();

// same
parser.AddArg<int>("integer").Required().Default(42);

// Default option is not acceptable
parser.AddArg<int>("integer").Options({0, 1}).Default(42);
```

# TODO
* global arguments
* support `--help` option
