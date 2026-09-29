// libc++ C++ feature autotest for the TCMIPS emulator.
//
// Aggregates the standalone C++ feature demos under test/cxx/ (basic, fmt,
// ctors, stringview, reduce, lcm) into a single AUTOTEST target that follows
// the same self-check harness as test_memfs_autotest.cpp /
// test_picolibc_autotest.c: every assertion is emitted as a single greppable
// line
//   [SELF] <name> PASS   /   [SELF] <name> FAIL (detail)
// and the process exits 0 when all checks pass, non-zero otherwise (reported by
// the emulator as "!! TCMIPS EMULATOR EXIT, code=N !!").
//
// Compiled with -std=c++17 (see test/cxx/CMakeLists.txt). Note that the
// original lcm.cpp used C++20 abbreviated function templates; here it is
// rewritten to the C++17 equivalent (std::lcm is a C++17 facility).
//
// IMPORTANT: the toolchain crosses libc++ with -fno-exceptions, so nothing here
// throws or relies on try/catch.

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#define FMT_HEADER_ONLY
#include "fmt/format.h"

// ---------------- autotest harness -------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

static void t_check(bool cond, const char *name, const char *detail) {
  if (cond) {
    g_pass++;
    std::printf("[SELF] %s PASS\n", name);
  } else {
    g_fail++;
    std::printf("[SELF] %s FAIL %s\n", name, detail ? detail : "");
  }
}

#define CHECK(expr, name) t_check((expr) ? true : false, name, NULL)

#define CHECK_EQ_LL(actual, expected, name)                                    \
  do {                                                                         \
    long long a_ = (long long)(actual);                                        \
    long long e_ = (long long)(expected);                                      \
    char d_[80];                                                               \
    std::snprintf(d_, sizeof(d_), "(actual=%lld expected=%lld)", a_, e_);      \
    t_check(a_ == e_, name, d_);                                               \
  } while (0)

// ============================================================================
// 1. Containers (vector / map / unordered_map / set / list)
// ============================================================================
static void test_containers(void) {
  std::vector<int> vec = {4, 1, 8, 3, 5};
  vec.push_back(9);
  CHECK_EQ_LL(vec.size(), 6, "vector size");
  CHECK(vec[5] == 9, "vector push_back value");

  std::map<std::string, int> age_map;
  age_map["Alice"] = 25;
  age_map["Bob"] = 30;
  CHECK_EQ_LL(age_map.size(), 2, "map size");
  CHECK(age_map["Alice"] == 25 && age_map["Bob"] == 30, "map key/value");
  // map is ordered by key: Alice < Bob.
  CHECK(age_map.begin()->first == "Alice", "map ordered by key");

  std::unordered_map<int, std::string> identity_map = {{1, "One"}, {2, "Two"}};
  auto it = identity_map.find(2);
  CHECK(it != identity_map.end() && it->second == "Two", "unordered_map find");

  std::set<int> s = {5, 1, 3, 3, 2};
  CHECK_EQ_LL(s.size(), 4, "set dedup size");
  CHECK(s.count(3) == 1, "set counts unique");

  std::list<int> lst = {10, 20, 30};
  CHECK_EQ_LL(lst.size(), 3, "list size");
}

// ============================================================================
// 2. Iterators (forward / reverse / const)
// ============================================================================
static void test_iterators(void) {
  std::list<int> my_list = {10, 20, 30, 40};

  int fwd_sum = 0;
  for (std::list<int>::iterator it = my_list.begin(); it != my_list.end(); ++it)
    fwd_sum += *it;
  CHECK_EQ_LL(fwd_sum, 100, "forward iterator traversal");

  int rev_sum = 0;
  for (std::list<int>::reverse_iterator rit = my_list.rbegin();
       rit != my_list.rend(); ++rit)
    rev_sum += *rit;
  CHECK_EQ_LL(rev_sum, 100, "reverse iterator traversal");

  int c_count = 0;
  for (std::list<int>::const_iterator cit = my_list.cbegin();
       cit != my_list.cend(); ++cit)
    c_count++;
  CHECK_EQ_LL(c_count, 4, "const iterator traversal");
}

// ============================================================================
// 3. Standard algorithms (sort / find / unique / accumulate)
// ============================================================================
static void test_algorithms(void) {
  std::vector<int> nums = {5, 2, 9, 1, 5, 6};

  std::sort(nums.begin(), nums.end());
  bool sorted = std::is_sorted(nums.begin(), nums.end());
  CHECK(sorted, "std::sort ascending");

  auto it = std::find(nums.begin(), nums.end(), 9);
  CHECK(it != nums.end(), "std::find hit");
  CHECK_EQ_LL(std::distance(nums.begin(), it), 5, "std::find index");

  auto last = std::unique(nums.begin(), nums.end());
  nums.erase(last, nums.end());
  CHECK_EQ_LL(nums.size(), 5, "std::unique removes dup");

  int sum = std::accumulate(nums.begin(), nums.end(), 0);
  CHECK_EQ_LL(sum, 23, "std::accumulate sum");
}

// ============================================================================
// 4. Functors & lambdas (greater / count_if)
// ============================================================================
static void test_functors(void) {
  std::vector<int> data = {1, 2, 3, 4, 5};

  std::sort(data.begin(), data.end(), std::greater<int>());
  CHECK(data.front() == 5 && data.back() == 1, "std::greater descending sort");

  auto is_even = [](int n) { return n % 2 == 0; };
  long even_count = std::count_if(data.begin(), data.end(), is_even);
  CHECK_EQ_LL(even_count, 2, "lambda count_if");

  // std::function wrapping a lambda.
  std::function<int(int)> inc = [](int x) { return x + 1; };
  CHECK(inc(41) == 42, "std::function lambda");
}

// ============================================================================
// 5. Smart pointers (unique_ptr / shared_ptr)
// ============================================================================
static void test_smart_pointers(void) {
  std::unique_ptr<int> u_ptr = std::make_unique<int>(100);
  CHECK(u_ptr && *u_ptr == 100, "unique_ptr value");

  std::shared_ptr<int> s_ptr1 = std::make_shared<int>(200);
  CHECK_EQ_LL(s_ptr1.use_count(), 1, "shared_ptr initial count");
  {
    std::shared_ptr<int> s_ptr2 = s_ptr1;
    CHECK_EQ_LL(s_ptr1.use_count(), 2, "shared_ptr ref count in scope");
  }
  CHECK_EQ_LL(s_ptr1.use_count(), 1, "shared_ptr ref count after scope");

  // weak_ptr observes without owning.
  std::weak_ptr<int> w = s_ptr1;
  CHECK(!w.expired(), "weak_ptr not expired");
}

// ============================================================================
// 6. Custom allocator concept
// ============================================================================
template <typename T> struct SimpleAllocator {
  using value_type = T;
  SimpleAllocator() = default;
  template <typename U>
  constexpr SimpleAllocator(const SimpleAllocator<U> &) noexcept {}

  T *allocate(std::size_t n) {
    return static_cast<T *>(::operator new(n * sizeof(T)));
  }
  void deallocate(T *p, std::size_t) noexcept { ::operator delete(p); }
};

static void test_allocator(void) {
  std::vector<int, SimpleAllocator<int>> custom_vec;
  custom_vec.push_back(10);
  custom_vec.push_back(20);
  CHECK_EQ_LL(custom_vec.size(), 2, "custom allocator vector size");
  CHECK(custom_vec[0] == 10 && custom_vec[1] == 20,
        "custom allocator vector content");
}

// ============================================================================
// 7. Static storage duration object lifecycle (ctor before main, dtor after)
// ============================================================================
// A global object with a construction count: if its constructor has run before
// main(), then C++ static initialization order is honored by the runtime. The
// local-static variant verifies lazy (function-local) initialization.
struct LifecycleProbe {
  int &counter;
  explicit LifecycleProbe(int &c) : counter(c) { counter++; }
};

static int g_ctor_count = 0;
static LifecycleProbe g_probe(g_ctor_count); // constructed before main

// A function-local static object whose constructor counts how many times it was
// actually constructed. Correct behavior: constructed exactly once, on first
// entry, and never re-initialized on subsequent entries.
static int &local_static_ctor_count() {
  static int runs = 0;
  struct Counter {
    Counter() { runs++; }
  };
  static Counter c; // constructed once
  return runs;
}

static void test_static_lifetime(void) {
  CHECK_EQ_LL(g_ctor_count, 1, "global object constructed before main");

  (void)local_static_ctor_count(); // first entry -> constructs
  (void)local_static_ctor_count(); // second entry -> no re-construction
  CHECK_EQ_LL(local_static_ctor_count(), 1,
              "function-local static constructed exactly once");
}

// ============================================================================
// 8. fmtlib (fmt::format header-only)
// ============================================================================
static void test_fmt(void) {
  std::string basic = fmt::format("Hello {}, welcome to C++{}!", "Dev", 20);
  CHECK(basic == "Hello Dev, welcome to C++20!", "fmt::format positional");

  int number = 42;
  std::string dec = fmt::format("{0:d}", number);
  CHECK(dec == "42", "fmt::format decimal");
  std::string hex = fmt::format("{0:x}", number);
  CHECK(hex == "2a", "fmt::format hex");
  std::string bin = fmt::format("{0:b}", number);
  CHECK(bin == "101010", "fmt::format binary");

  double pi = 3.1415926535;
  std::string p = fmt::format("{:.6f}", pi);
  CHECK(p == "3.141593", "fmt::format float precision");
}

// ============================================================================
// 8. string_view
// ============================================================================
static void test_string_view(void) {
  std::string_view sv = "hello";
  CHECK_EQ_LL(sv.size(), 5, "string_view size");
  CHECK(sv == "hello", "string_view compare");
  CHECK(sv.substr(1, 3) == "ell", "string_view substr");

  const char *raw = "world";
  std::string_view v2(raw, 5);
  CHECK(v2 == "world", "string_view from ptr+len");

  // remove_prefix / remove_suffix (non-mutating view).
  std::string_view v3 = "abcdef";
  v3.remove_prefix(2);
  v3.remove_suffix(1);
  CHECK(v3 == "cde", "string_view remove prefix/suffix");
}

// ============================================================================
// 9. std::reduce (C++17) vs std::accumulate
// ============================================================================
static void test_reduce(void) {
  const std::vector<double> vd(1007, 0.1);
  double acc = std::accumulate(vd.cbegin(), vd.cend(), 0.0);
  double red = std::reduce(vd.cbegin(), vd.cend());
  // Both compute the same sum (within tolerance for double reordering).
  const double diff = acc - red;
  CHECK(diff > -0.1 && diff < 0.1, "reduce ~= accumulate (double)");

  const std::vector<long> vl(1007, 1);
  long acc_l = std::accumulate(vl.cbegin(), vl.cend(), 0L);
  long red_l = std::reduce(vl.cbegin(), vl.cend());
  CHECK_EQ_LL(acc_l, 1007, "accumulate sum (long)");
  CHECK_EQ_LL(red_l, 1007, "reduce sum (long)");

  // reduce with initial value and binary op.
  long ten = std::reduce(vl.cbegin(), vl.cend(), 0L, std::plus<long>());
  CHECK_EQ_LL(ten, 1007, "reduce with plus op");
}

// ============================================================================
// 10. std::lcm / std::gcd (C++17 <numeric>)
// ============================================================================
static void test_lcm_gcd(void) {
  static_assert(std::lcm(6, 10) == 30, "lcm(6,10)");
  static_assert(std::lcm(6, -10) == 30, "lcm(6,-10)");
  static_assert(std::lcm(-6, -10) == 30, "lcm(-6,-10)");
  static_assert(std::lcm(24, 0) == 0, "lcm(24,0)");

  static_assert(std::gcd(12, 18) == 6, "gcd(12,18)");
  static_assert(std::gcd(7, 5) == 1, "gcd coprime");

  // Runtime checks.
  CHECK_EQ_LL(std::lcm(45, 75), 225, "lcm(45,75)");
  CHECK_EQ_LL(std::gcd(54, 24), 6, "gcd(54,24)");

  // Variadic lcm reduced to nested binary std::lcm (C++17 std::lcm is binary;
  // the original C++20 demo used an abbreviated variadic template).
  int a = std::lcm(std::lcm(2 * 3, 3 * 4), 4 * 5);
  CHECK_EQ_LL(a, 60, "lcm(6,12,20)");
  int b = std::lcm(std::lcm(2 * 3 * 4, 3 * 4 * 5), 4 * 5 * 6);
  CHECK_EQ_LL(b, 120, "lcm(24,60,120)");
}

// ============================================================================
// main
// ============================================================================
int main(void) {
  std::printf("=== cxx standard-library AUTOTEST ===\n");

  test_containers();
  test_iterators();
  test_algorithms();
  test_functors();
  test_smart_pointers();
  test_allocator();
  test_static_lifetime();
  test_fmt();
  test_string_view();
  test_reduce();
  test_lcm_gcd();

  std::printf("\n[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  std::printf("%s\n", g_fail == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
  return g_fail == 0 ? 0 : 1;
}
