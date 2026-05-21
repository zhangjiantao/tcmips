//
// Created by zhangjiantao on 2026/5/21.
//

/*
[Construct] File scope global object
[Construct] File static global object
[Construct] Namespace scope global object
[Construct] Class static member object

==== Enter main() ====

[Construct] Function local static object

==== Leave main() ====
[Destruct]  Function local static object
[Destruct]  Class static member object
[Destruct]  Namespace scope global object
[Destruct]  File static global object
[Destruct]  File scope global object
*/

#include <iostream>
#include <string>

struct TestInstance {
  std::string name;

  explicit TestInstance(std::string n) : name(std::move(n)) {
    std::cout << "[Construct] " << name << "\n";
  }

  ~TestInstance() { std::cout << "[Destruct]  " << name << "\n"; }
};

// 1. Global variable at file scope
TestInstance g_file_global("File scope global object");

// 2. File static global
static TestInstance g_file_static("File static global object");

// 3. Namespace scope global
namespace DemoNS {
TestInstance g_ns_global("Namespace scope global object");
}

// 4. Class static member
struct DemoClass {
  static TestInstance s_static_member;
};
TestInstance DemoClass::s_static_member("Class static member object");

int main() {
  std::cout << "\n==== Enter main() ====\n\n";

  // 5. Local static inside function (lazy init on first use)
  static TestInstance func_local_static("Function local static object");

  std::cout << "\n==== Leave main() ====\n";
  return 0;
}
