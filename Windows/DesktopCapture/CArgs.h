#pragma once

#include <iostream>

struct CArgs {
  static void showArg(int argc, char** argv, std::ostream& os = std::cout);
  static void showEnv(char** env, std::ostream& os = std::cout);
};
