//
// Created by zhangjiantao on 2026/5/21.
//

// https://en.cppreference.com/cpp/string/basic_string_view

#include <iostream>
#include <string_view>

int main() {
#define A "\xdf"
#define B "\xdc"
#define C "\xc4"

  constexpr std::string_view blocks[]{A B C, B A C, A C B, B C A};

  for (int y{}, p{}; y != 24; ++y, p = ((p + 1) % 4)) {
    for (char x{}; x != 26; ++x)
      std::cout << blocks[p];
    std::cout << '\n';
  }
}
