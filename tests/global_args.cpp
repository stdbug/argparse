#include "argparse/argparse.h"

argparse::FlagHolderWrapper boolean = argparse::AddGlobalFlag("boolean", 'b');
extern ::argparse::ArgHolderWrapper<int> integer =
    argparse::AddGlobalArg<int>("integer", 'i');
extern ::argparse::MultiArgHolderWrapper<double> doubles =
    argparse::AddGlobalMultiArg<double>("doubles", 'd');
