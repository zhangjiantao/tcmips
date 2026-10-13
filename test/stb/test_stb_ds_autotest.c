// stb_ds.h dynamic arrays & hash maps AUTOTEST.
//
// Exercises public macros only:
//   arrput / arrlen / arrpop / arrlast / arrdel / arrfree  — dynamic arrays
//   hmput / hmget / hmgeti / hmlen / hmgetp / hmdel       — typed hash maps
//   arrpush (alias for arrput), arrdeln (delete n elements)
// Pure computation, no external assets.
//
// All assertions follow the autotest convention:
//   [SELF] <name> PASS   /   [SELF] <name> FAIL (detail)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stb/stb_ds.h"

// ---------------- autotest harness -------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

static void t_check(int cond, const char *name, const char *detail) {
  if (cond) {
    g_pass++;
    printf("[SELF] %s PASS\n", name);
  } else {
    g_fail++;
    printf("[SELF] %s FAIL %s\n", name, detail ? detail : "");
  }
}

#define CHECK(cond, name) t_check((cond) ? 1 : 0, name, NULL)

#define CHECK_EQ(actual, expected, name)                                       \
  do {                                                                         \
    long long a_ = (long long)(actual);                                        \
    long long e_ = (long long)(expected);                                     \
    char d_[96];                                                               \
    snprintf(d_, sizeof(d_), "(actual=%lld expected=%lld)", a_, e_);           \
    t_check(a_ == e_, name, d_);                                               \
  } while (0)

// Typed hash map: int key → int value.  Must be a struct with {int key; int value;}
// fields for hmput/hmget/hmgeti etc.
typedef struct { int key; int value; } hm_ii_t;

// ---------------- dynamic array tests ----------------------------------------

static int test_arr_basic(void) {
  int *arr = NULL;
  int i;

  // Empty: arrlen returns 0 (also alias arrpush/arrpush).
  CHECK(arrlen(arr) == 0, "arr:len_empty");

  // Push 5 integers via arrput (arrpush is synonym).
  for (i = 0; i < 5; i++) {
    arrpush(arr, i * 10);
  }
  CHECK(arrlen(arr) == 5, "arr:len_5");
  CHECK(arr[0] == 0 && arr[1] == 10 && arr[2] == 20 && arr[3] == 30 && arr[4] == 40,
        "arr:values");

  // arrlast returns pointer to last element.
  CHECK(arrlast(arr) == 40, "arr:last");

  // arrpop returns the popped value and decrements length.
  int popped = arrpop(arr);
  CHECK(popped == 40, "arr:popped_value");
  CHECK(arrlen(arr) == 4, "arr:len_after_pop");

  // Pop remaining elements one-by-one.
  arrpop(arr); arrpop(arr); arrpop(arr); arrpop(arr);
  CHECK(arrlen(arr) == 0, "arr:len_empty_after_pops");

  // Push after full pop cycle (verifies no state corruption).
  arrput(arr, 100);
  CHECK(arrlen(arr) == 1 && arr[0] == 100, "arr:push_after_cycle");

  // Large array growth (capacity doubling, reallocs).
  // arr currently holds [100] (len==1) from the pop-cycle test above, so after
  // pushing large_n more elements the array is [100, 0, 1, ..., large_n-1].
  const int large_n = 1000;
  for (i = 0; i < large_n; i++) {
    arrput(arr, i);
  }
  CHECK(arrlen(arr) == (size_t)(1 + large_n), "arr:len_large");
  CHECK(arr[0] == 100 && arr[1 + large_n / 2] == large_n / 2 &&
        arr[large_n] == large_n - 1, "arr:large_values");

  arrfree(arr);
  CHECK(arr == NULL && arrlen(arr) == 0, "arr:freed_and_null");
  return 1;
}

// ---------------- dynamic array operations -----------------------------------

static int test_arr_advanced(void) {
  int *arr = NULL;
  int i;

  // Build a 10-element array.
  for (i = 0; i < 10; i++) {
    arrput(arr, i * 11);
  }
  CHECK(arrlen(arr) == 10, "arradv:len");

  // arrdel: delete index 3 (1 element).
  arrdel(arr, 3);
  CHECK(arrlen(arr) == 9, "arradv:len_after_del");
  CHECK(arr[3] == 44 && arr[9] == 99, "arradv:shift_after_del");

  // arrdeln: delete 3 elements starting at index 2.
  // After arrdel(arr,3): [0,11,22,44,55,66,77,88,99] (9 elts).
  // Delete idx 2,3,4 (22,44,55) -> [0,11,66,77,88,99] (6 elts).
  arrdeln(arr, 2, 3);
  CHECK(arrlen(arr) == 6, "arradv:len_after_deln");
  CHECK(arr[2] == 66 && arr[5] == 99, "arradv:after_deln");

  // arrins: insert at index 1.
  // [0,11,66,77,88,99] -> insert 999 at 1 -> [0,999,11,66,77,88,99] (7 elts).
  arrins(arr, 1, 999);
  CHECK(arrlen(arr) == 7, "arradv:len_after_ins");
  CHECK(arr[1] == 999 && arr[2] == 11, "arradv:insert_values");

  arrfree(arr);
  return 1;
}

// ---------------- hash map tests (int key → int value) -----------------------

static int test_hmap_int(void) {
  hm_ii_t *map = NULL;
  int idx;

  // Empty.
  CHECK(hmlen(map) == 0, "hmap:len_empty");

  // Single entry.
  hmput(map, 42, 100);
  CHECK(hmlen(map) == 1, "hmap:len_1");
  idx = hmgeti(map, 42);
  CHECK(idx >= 0 && map[idx].key == 42 && map[idx].value == 100,
        "hmap:get_existing");

  // hmget by exact key returns value.
  int val = hmget(map, 42);
  CHECK(val == 100, "hmap:hmget");

  // Missing key: hmgeti returns -1; hmget returns default (0 from struct init).
  CHECK(hmgeti(map, 99) == -1, "hmap:get_missing");

  // Overwrite existing key keeps count at 1.
  hmput(map, 42, 200);
  idx = hmgeti(map, 42);
  CHECK(idx >= 0 && map[idx].value == 200, "hmap:overwrite");
  CHECK(hmlen(map) == 1, "hmap:len_1_after_overwrite");

  // Multiple distinct keys — capture indices first to avoid unsequenced
  // map mutation within the same CHECK expression (hmgeti may reassign map).
  hmput(map, 7, 700);
  hmput(map, 13, 1300);
  hmput(map, 99, 9900);
  CHECK(hmlen(map) == 4, "hmap:len_4");
  int i7 = hmgeti(map, 7);
  CHECK(i7 >= 0 && map[i7].value == 700, "hmap:get_7");
  int i13 = hmgeti(map, 13);
  CHECK(i13 >= 0 && map[i13].value == 1300, "hmap:get_13");
  int i99 = hmgeti(map, 99);
  CHECK(i99 >= 0 && map[i99].value == 9900, "hmap:get_99");

  // hmgetp returns pointer to struct for existing key, NULL for missing.
  hm_ii_t *found = hmgetp(map, 13);
  CHECK(found != NULL && found->key == 13 && found->value == 1300, "hmap:pointer");

  // hmdel deletes existing key (returns 1) and missing key (returns 0).
  CHECK(hmdel(map, 99) == 1, "hmap:del_existing");
  CHECK(hmlen(map) == 3, "hmap:len_3_after_del");
  // hmgeti for a deleted key must be -1 (recompute separately).
  int i99_after = hmgeti(map, 99);
  CHECK(i99_after == -1, "hmap:key_gone_after_del");
  CHECK(hmdel(map, 99) == 0, "hmap:del_already_gone");

  // Negative and zero keys — capture indices first to avoid unsequenced access.
  hmput(map, -1, -100);
  hmput(map, 0, 0);
  int ineg = hmgeti(map, -1);
  CHECK(ineg >= 0 && map[ineg].value == -100, "hmap:negative_key");
  int izero = hmgeti(map, 0);
  CHECK(izero >= 0 && map[izero].value == 0 && map[izero].key == 0, "hmap:zero_key");

  hmfree(map);
  CHECK(map == NULL, "hmap:freed_to_null");
  return 1;
}

// ---------------- array of structs -------------------------------------------

static int test_arr_structs(void) {
  typedef struct { int id; float score; } rec_t;
  rec_t *recs = NULL;
  int i;

  // Push 10 records, verify all by index.
  for (i = 0; i < 10; i++) {
    rec_t r;
    r.id = i;
    r.score = (float)(i * 1.5);
    arrput(recs, r);
  }
  CHECK(arrlen(recs) == 10, "arrstruct:len");
  CHECK(recs[0].id == 0 && recs[9].id == 9, "arrstruct:ids");
  CHECK(recs[5].id == 5 && recs[5].score == 7.5f, "arrstruct:score");
  CHECK(arrlast(recs).id == 9, "arrstruct:last");

  // arrdel removes at index 3 and shifts left.
  arrdel(recs, 3);
  CHECK(arrlen(recs) == 9, "arrstruct:len_after_del");
  CHECK(recs[3].id == 4 && recs[8].id == 9, "arrstruct:shift_after_del");

  arrfree(recs);
  return 1;
}

int main(void) {
  printf("=== stb_ds AUTOTEST ===\n");

  test_arr_basic();
  test_arr_advanced();
  test_hmap_int();
  test_arr_structs();

  printf("[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  return g_fail > 0 ? 1 : 0;
}
