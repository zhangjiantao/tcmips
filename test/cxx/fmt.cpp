#include <iostream>
#include <string>

#define FMT_HEADER_ONLY
#include "fmt/format.h"

int main() {
  std::cout << "=== TCMIPS fmtlib Compatibility Test ===\n\n";

  std::string basic =
      fmt::format("Hello {}, welcome to C++{}!\n", "Developer", 20);
  std::cout << basic;

  int number = 42;
  std::cout << fmt::format("Decimal: {0:d} | Hex: 0x{0:x} | Binary: 0b{0:b}\n",
                           number);

  double pi = 3.1415926535;
  std::cout << fmt::format("Pi precision: {:.6f}\n", pi);

  return 0;
}
