#include "argparse/argparse.h"

#include <stdio.h>

auto str = argparse::AddGlobalArg<std::string>("string", "Some string");

int main(int argc, char* argv[]) {
  argparse::Parser parser;
  auto helpstring = parser.AddArg<std::string>("helpstring");
  auto output = parser.AddArg<std::string>("output").Required();
  parser.IgnoreGlobalFlags();
  parser.ParseArgs(argc, argv);

  if (freopen(output->c_str(), "wb", stderr) == nullptr) {
    std::cerr << "Failed to redirect stderr\n";
    return EXIT_FAILURE;
  }

  argparse::Parser test_parser;
  test_parser.AddPositionalArg<float>();
  test_parser.AddPositionalArg<int>();
  test_parser.AddArg<int>("integer", 'i', "Some integer")
      .Required()
      .Options({1, 2, 3});
  test_parser.AddArg<int>("integer2", 'j', "Another integer")
      .Options({5, 6, 7})
      .Default(42);

  if (helpstring) {
    test_parser.ExitOnFailure(EXIT_SUCCESS, *helpstring);
  } else {
    test_parser.ExitOnFailure(EXIT_SUCCESS);
  }

  test_parser.ParseArgs(std::vector<std::string>(1, "binary"));

  // Should never get here
  return EXIT_FAILURE;
}
