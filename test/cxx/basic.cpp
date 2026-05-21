//
// Created by zhangjiantao on 2026/5/13.
//

#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// Visual Anchor: Prints a scannable header for each section
void print_header(const std::string &title) {
  std::cout << "\n=== " << title << " ===\n";
}

// 1. Testing Containers
void test_containers() {
  print_header("1. Container Testing");

  // Sequence container: vector
  std::vector<int> vec = {4, 1, 8, 3, 5};
  vec.push_back(9);
  std::cout << "Vector elements: ";
  for (int x : vec)
    std::cout << x << " ";
  std::cout << "\n";

  // Associative container: map (ordered by key)
  std::map<std::string, int> age_map;
  age_map["Alice"] = 25;
  age_map["Bob"] = 30;
  std::cout << "Ordered Map elements:\n";
  for (const auto &[name, age] :
       age_map) { // C++17 Structured Binding [^std-c17]
    std::cout << "  " << name << ": " << age << "\n";
  }

  // Unordered associative container: unordered_map (hash table)
  std::unordered_map<int, std::string> identity_map = {{1, "One"}, {2, "Two"}};

  // Fixed: Correctly lookup by key using find() to avoid compilation error
  auto it = identity_map.find(2);
  if (it != identity_map.end()) {
    std::cout << "Unordered_Map lookup (key=2): " << it->second << "\n";
  }
}

// 2. Testing Iterators
void test_iterators() {
  print_header("2. Iterator Testing");

  std::list<int> my_list = {10, 20, 30, 40};

  // Forward Iterator: Standard read-write traversal
  std::cout << "List forward traversal:  ";
  for (std::list<int>::iterator it = my_list.begin(); it != my_list.end();
       ++it) {
    std::cout << *it << " ";
  }
  std::cout << "\n";

  // Reverse Iterator: Traversing from end to begin
  std::cout << "List reverse traversal:  ";
  for (std::list<int>::reverse_iterator rit = my_list.rbegin();
       rit != my_list.rend(); ++rit) {
    std::cout << *rit << " ";
  }
  std::cout << "\n";

  // Const Iterator: Read-only traversal
  std::cout << "List const traversal:    ";
  for (std::list<int>::const_iterator cit = my_list.cbegin();
       cit != my_list.cend(); ++cit) {
    std::cout << *cit << " ";
  }
  std::cout << "\n";
}

// 3. Testing Standard Algorithms
void test_algorithms() {
  print_header("3. Algorithm Testing");

  std::vector<int> nums = {5, 2, 9, 1, 5, 6};

  // Sorting: std::sort
  std::sort(nums.begin(), nums.end());
  std::cout << "Sorted vector: ";
  for (int n : nums)
    std::cout << n << " ";
  std::cout << "\n";

  // Searching: std::find
  auto it = std::find(nums.begin(), nums.end(), 9);
  if (it != nums.end()) {
    std::cout << "Found element 9 at index: " << std::distance(nums.begin(), it)
              << "\n";
  }

  // Removing duplicates: std::unique (requires pre-sorted container)
  auto last = std::unique(nums.begin(), nums.end());
  nums.erase(last, nums.end());
  std::cout << "Unique elements: ";
  for (int n : nums)
    std::cout << n << " ";
  std::cout << "\n";

  // Numeric Accumulation: std::accumulate
  int sum = std::accumulate(nums.begin(), nums.end(), 0);
  std::cout << "Sum of elements: " << sum << "\n";
}

// 4. Testing Functors & Lambda Expressions
void test_functors() {
  print_header("4. Functors & Lambdas");

  std::vector<int> data = {1, 2, 3, 4, 5};

  // Built-in Functor: std::greater (for descending order)
  std::sort(data.begin(), data.end(), std::greater<int>());
  std::cout << "Sorted descending (via std::greater): ";
  for (int x : data)
    std::cout << x << " ";
  std::cout << "\n";

  // Lambda expression as a predicate (counting even numbers)
  auto is_even = [](int n) { return n % 2 == 0; };
  long even_count = std::count_if(data.begin(), data.end(), is_even);
  std::cout << "Count of even numbers: " << even_count << "\n";
}

// 5. Testing Smart Pointers (Memory Management)
void test_smart_pointers() {
  print_header("5. Smart Pointers");

  // unique_ptr: Unique ownership
  std::unique_ptr<int> u_ptr = std::make_unique<int>(100);
  std::cout << "unique_ptr value: " << *u_ptr << "\n";

  // shared_ptr: Shared ownership with reference counting
  std::shared_ptr<int> s_ptr1 = std::make_shared<int>(200);
  {
    std::shared_ptr<int> s_ptr2 = s_ptr1;
    std::cout << "Inside local scope, shared_ptr ref count: "
              << s_ptr1.use_count() << "\n";
  }
  std::cout << "Outside local scope, shared_ptr ref count: "
            << s_ptr1.use_count() << "\n";
}

// 6. Testing Custom Allocator Concept
template <typename T> struct SimpleAllocator {
  using value_type = T;
  SimpleAllocator() = default;
  template <typename U>
  constexpr SimpleAllocator(const SimpleAllocator<U> &) noexcept {}

  T *allocate(std::size_t n) {
    std::cout << "[Alloc] Allocated " << n * sizeof(T) << " bytes.\n";
    return static_cast<T *>(::operator new(n * sizeof(T)));
  }
  void deallocate(T *p, std::size_t n) noexcept {
    std::cout << "[Alloc] Deallocated " << n * sizeof(T) << " bytes.\n";
    ::operator delete(p);
  }
};

void test_allocator() {
  print_header("6. Custom Allocator");

  // Injecting our custom allocator into a standard vector
  std::vector<int, SimpleAllocator<int>> custom_vec;
  custom_vec.push_back(10); // Triggers internal memory allocation/resize
  custom_vec.push_back(20);
}

int main() {
  while (1) {
    std::cout << "========================================\n";
    std::cout << "   C++ STL FEATURES COMPREHENSIVE TEST  \n";
    std::cout << "========================================\n";

    test_containers();
    usleep(3000000);
    test_iterators();
    usleep(3000000);
    test_algorithms();
    usleep(3000000);
    test_functors();
    usleep(3000000);
    test_smart_pointers();
    usleep(3000000);
    test_allocator();
    usleep(3000000);

    std::cout << "\n========================================\n";
    std::cout << "             TEST COMPLETED             \n";
    std::cout << "========================================\n";
  }
  return 0;
}
