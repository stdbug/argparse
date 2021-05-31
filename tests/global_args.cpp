#include "argparse/argparse.h"

ARGPARSE_DEFINE_GLOBAL_FLAG(boolean, "boolean", 'b', "");
ARGPARSE_DEFINE_GLOBAL_ARG(int, integer, "integer", 'i', "");
ARGPARSE_DEFINE_GLOBAL_MULTIARG(double, doubles, "doubles", 'd', "");
