// libc++ <filesystem> autotest for libmemfs under the TCMIPS emulator.
//
// This is the C++ counterpart of test_memfs_autotest.c: it exercises the
// standard <filesystem> layer (libc++ 14) against the same in-memory FS
// populated from test/memfs/assets by --tcm-memfs=<dir>. It emits the same
// machine-readable lines
//   [SELF] <name> PASS   /   [SELF] <name> FAIL
// and exits with code 0 when every check passed, non-zero otherwise (reported
// by the emulator as "!! TCMIPS EMULATOR EXIT, code=N !!").
//
// IMPORTANT: the TCMIPS toolchain crosses libc++ with -fno-exceptions. Any
// operation that would throw (reachable in the standard configuration) instead
// calls abort() and crashes the guest. We therefore NEVER rely on try/catch:
// every query that can fail for a missing/invalid path is done through the
// error_code overload, which reports failure without throwing. Normal
// operations on known-good paths are called directly.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace fs = std::__fs::filesystem;

// ---------------- autotest harness ------------------------------------------

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

// ============================================================================
// 1. path parsing (pure, no filesystem calls)
// ============================================================================
static void test_path_parsing(void) {
  std::printf("\n== 1. path parsing ==\n");
  fs::path p = "levels/l1.conf";

  CHECK(p.filename() == "l1.conf", "filename() = l1.conf");
  CHECK(p.extension() == ".conf", "extension() = .conf");
  CHECK(p.stem() == "l1", "stem() = l1");
  CHECK(p.parent_path() == "levels", "parent_path() = levels");
  CHECK(p.parent_path().filename() == "levels", "parent filename = levels");

  CHECK(p.is_relative(), "l1.conf path is relative");
  CHECK(!p.is_absolute(), "relative path is not absolute");
  CHECK(fs::path("/x").is_absolute(), "absolute path is absolute");

  fs::path joined = fs::path("levels") / "l1.conf";
  CHECK(joined == p, "operator/ builds levels/l1.conf");

  CHECK(fs::path("a/./b").lexically_normal() == fs::path("a/b"),
        "lexically_normal removes ./");
  CHECK(fs::path("a/../b").lexically_normal() == fs::path("b"),
        "lexically_normal removes ../");
}

// ============================================================================
// 2. existence / type queries against the packed assets
// ============================================================================
static void test_status_queries(void) {
  std::printf("\n== 2. status & type queries ==\n");
  fs::path cfg = "config.json";
  fs::path hello = "hello.txt";
  fs::path l1 = "levels/l1.conf";
  fs::path levels = "levels";

  CHECK(fs::exists(cfg), "config.json exists");
  CHECK(fs::exists(hello), "hello.txt exists");
  CHECK(fs::exists(l1), "levels/l1.conf exists");
  CHECK(fs::exists(levels), "levels exists");
  CHECK(!fs::exists("no_such_asset.xyz"), "missing asset does not exist");

  CHECK(fs::is_regular_file(cfg), "config.json is regular file");
  CHECK(fs::is_regular_file(hello), "hello.txt is regular file");
  CHECK(fs::is_regular_file(l1), "l1.conf is regular file");
  CHECK(!fs::is_regular_file(levels), "levels is not a regular file");
  CHECK(fs::is_directory(levels), "levels is a directory");
  CHECK(!fs::is_directory(cfg), "config.json is not a directory");

  CHECK(fs::status(cfg).type() == fs::file_type::regular,
        "status(config.json).type() == regular");
  CHECK(fs::status(levels).type() == fs::file_type::directory,
        "status(levels).type() == directory");

  // For a missing path the ec overload reports failure (sets ec) and yields a
  // non-regular status; it must never throw.
  std::error_code ec;
  fs::file_status st = fs::status("missing.zzz", ec);
  t_check(ec.value() != 0 && st.type() == fs::file_type::not_found,
          "status(missing, ec) sets ec and reports not_found", "");

  CHECK(fs::file_size(cfg) == 48, "config.json size 48");
  CHECK(fs::file_size(hello) == 13, "hello.txt size 13");
  CHECK(fs::file_size(l1) == 70, "l1.conf size 70");
}

// ============================================================================
// 3. directory traversal (single + recursive) over the packed tree
// ============================================================================
static void test_directory_iteration(void) {
  std::printf("\n== 3. directory traversal ==\n");

  bool seen_cfg = false, seen_hello = false, seen_levels = false, seen_root = false;
  for (const auto &e : fs::directory_iterator(".")) {
    auto nm = e.path().filename().string();
    if (nm == "config.json")
      seen_cfg = true;
    else if (nm == "hello.txt")
      seen_hello = true;
    else if (nm == "levels")
      seen_levels = true;
    else if (nm == "root.txt")
      seen_root = true;
    if (nm == "levels")
      CHECK(e.is_directory(), "iterator: levels entry is a directory");
  }
  CHECK(seen_cfg, "directory_iterator sees config.json");
  CHECK(seen_hello, "directory_iterator sees hello.txt");
  CHECK(seen_levels, "directory_iterator sees levels dir");
  CHECK(seen_root, "directory_iterator sees root.txt");

  bool seen_deep = false;
  for (const auto &e : fs::recursive_directory_iterator(".")) {
    if (e.path().filename() == "l1.conf")
      seen_deep = true;
  }
  CHECK(seen_deep, "recursive_directory_iterator finds levels/l1.conf");

  fs::create_directory("empty_iter");
  std::size_t count = 0;
  for (auto &&_ : fs::directory_iterator("empty_iter")) {
    (void)_;
    count++;
  }
  CHECK(count == 0, "empty dir iterates to nothing");
  fs::remove("empty_iter");
}

// ============================================================================
// 4. create / write / read / rename / remove
// ============================================================================
static void test_file_ops(void) {
  std::printf("\n== 4. create / write / rename / remove ==\n");

  fs::path dir = "cpp_out/sub/deep";
  CHECK(fs::create_directories(dir), "create_directories(cpp_out/sub/deep)");
  CHECK(fs::is_directory(dir), "nested dir exists after create_directories");
  CHECK(!fs::create_directory(dir),
        "create_directory on existing returns false");

  fs::path f = dir / "data.txt";
  {
    std::ofstream ofs(f.c_str());
    ofs << "fs-autotest-payload-42";
    CHECK(ofs.good(), "ofstream open+write ok");
  }
  {
    std::ifstream ifs(f.c_str());
    std::string line;
    std::getline(ifs, line);
    CHECK(line == "fs-autotest-payload-42", "ifstream reads back exact payload");
  }
  CHECK(fs::file_size(f) == 22, "written file size 22");

  fs::path renamed = dir / "renamed.txt";
  fs::rename(f, renamed); // known-good rename; must not crash
  CHECK(!fs::exists(f), "old name gone after rename");
  CHECK(fs::exists(renamed), "new name exists after rename");

  CHECK(!fs::is_empty(renamed), "non-empty file is not empty");
  // dir now contains renamed.txt, so it is not empty.
  CHECK(!fs::is_empty(dir), "dir with a child reports not empty");

  CHECK(fs::remove(renamed), "remove(renamed.txt)");
  CHECK(!fs::exists(renamed), "removed file is gone");
  std::uintmax_t n = fs::remove_all("cpp_out");
  CHECK(n > 0, "remove_all(cpp_out) removed entries");
  CHECK(!fs::exists("cpp_out"), "cpp_out gone after remove_all");
}

// ============================================================================
// 5. symlinks (create_symlink / is_symlink / read through)
// ============================================================================
static void test_symlinks(void) {
  std::printf("\n== 5. symlinks ==\n");

  fs::create_symlink(fs::path("/hello.txt"), fs::path("cpp_hello_link"));
  CHECK(fs::is_symlink("cpp_hello_link"), "is_symlink(cpp_hello_link)");
  CHECK(fs::is_regular_file("cpp_hello_link"), "followed link is regular");
  CHECK(fs::file_size("cpp_hello_link") == 13,
        "link follows to hello.txt (13 bytes)");

  {
    std::ifstream ifs("cpp_hello_link");
    std::string line;
    std::getline(ifs, line);
    CHECK(line == "Hello, memfs!", "read through symlink yields hello.txt");
  }

  CHECK(fs::symlink_status("cpp_hello_link").type() == fs::file_type::symlink,
        "symlink_status().type() == symlink");
  CHECK(fs::status("cpp_hello_link").type() == fs::file_type::regular,
        "status().type() == regular (followed)");

  fs::remove("cpp_hello_link");
  CHECK(!fs::exists("cpp_hello_link"), "symlink removed");
}

// ============================================================================
// 6. permissions
// ============================================================================
static void test_permissions(void) {
  std::printf("\n== 6. permissions ==\n");

  fs::create_directory("permdir");
  fs::path f = "permdir/p.txt";
  {
    std::ofstream ofs(f.c_str());
    ofs << "perms";
  }

  fs::permissions(f, fs::perms::owner_write | fs::perms::owner_read,
                  fs::perm_options::replace);
  fs::perms prm = fs::status(f).permissions();
  CHECK((prm & fs::perms::owner_write) != fs::perms::none, "owner_write set");
  CHECK((prm & fs::perms::owner_read) != fs::perms::none, "owner_read set");
  CHECK((prm & fs::perms::others_read) == fs::perms::none,
        "others_read cleared after replace");

  fs::remove_all("permdir");
  CHECK(!fs::exists("permdir"), "perm dir cleaned up");
}

// ============================================================================
// 7. absolute / canonical / weakly_canonical / current_path
// ============================================================================
static void test_path_resolution(void) {
  std::printf("\n== 7. absolute / canonical / cwd ==\n");

  CHECK(fs::absolute("config.json").is_absolute(), "absolute() yields absolute");
  CHECK(fs::current_path() == "/", "current_path() is / (memfs cwd)");

  // canonical() resolves the real node of an existing packed file.
  CHECK(!fs::canonical("levels/l1.conf").empty(), "canonical(l1.conf) resolves");

  // weakly_canonical tolerates a trailing nonexistent name.
  CHECK(!fs::weakly_canonical("levels/newlake.bin").empty(),
        "weakly_canonical ok with missing leaf");
}

// ============================================================================
// 8. space / temp dir / misc (all via ec overloads where they can fail)
// ============================================================================
static void test_space_misc(void) {
  std::printf("\n== 8. space & misc ==\n");

  fs::space_info si = fs::space("/");
  CHECK(si.capacity > 0, "space().capacity > 0");

  // start.c creates /tmp and exports TMPDIR so temp_directory_path resolves.
  std::error_code ec;
  fs::path tmp = fs::temp_directory_path(ec);
  t_check(!ec && !tmp.empty(), "temp_directory_path(ec) resolves to /tmp", "");
  CHECK(tmp == "/tmp", "temp directory is /tmp");
}

// ============================================================================
// 9. error handling: ec overloads report failure instead of throwing
// ============================================================================
static void test_errors(void) {
  std::printf("\n== 9. error handling (no crash) ==\n");

  // file_size on a missing path must set ec (and not crash).
  std::error_code ec;
  std::uintmax_t sz = fs::file_size("missing_for_size.bin", ec);
  t_check(ec.value() != 0 && sz == static_cast<std::uintmax_t>(-1),
          "file_size(missing, ec) sets error and returns -1", "");

  // directory_iterator on a regular file must set ec, not crash.
  ec.clear();
  {
    fs::directory_iterator it(fs::path("config.json"), ec);
    t_check(ec.value() != 0, "directory_iterator on a file sets ec", "");
  }

  // exists() is noexcept-ish and returns false harmlessly for missing paths.
  CHECK(!fs::exists("definitely_missing", ec), "exists(missing, ec) false");
}

// ============================================================================
int main() {
  std::printf("=== memfs libc++ filesystem AUTOTEST ===\n");

  test_path_parsing();
  test_status_queries();
  test_directory_iteration();
  test_file_ops();
  test_symlinks();
  test_permissions();
  test_path_resolution();
  test_space_misc();
  test_errors();

  std::printf("[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  std::printf("%s\n", g_fail == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
  return g_fail == 0 ? 0 : 1; // emulator reports this as "!! TCMIPS EMULATOR EXIT, code=N !!"
}