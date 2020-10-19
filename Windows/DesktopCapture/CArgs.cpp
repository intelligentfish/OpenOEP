#include "CArgs.h"

void CArgs::showArg(int argc, char** argv, std::ostream& os) {
  os << "args:" << std::endl;
  for (auto i = 0; i < argc; i++) {
    os << argv[i];
    if (i < argc - 1) os << ',';
  }
  os << std::endl;
}
void CArgs::showEnv(char** env, std::ostream& os) {
  os << "env:" << std::endl;
  while (env && *env) os << *env++ << std::endl;
}