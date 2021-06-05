#include "argparse/argparse.h"

auto boolean = argparse::AddGlobalFlag("boolean", 'b');
auto integer = argparse::AddGlobalArg<int>("integer", 'i');
auto doubles = argparse::AddGlobalMultiArg<double>("doubles", 'd');
