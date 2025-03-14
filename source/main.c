#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "coroed/api/log.h"

struct test {
  const char* name;
  void (*rountine)();
};

void test_counter();
void test_event();
void test_print();
void test_asyncio();

int main() {
  log_init();

  struct test tests[] = {
      {"counter", test_counter},
      {  "event",   test_event},
      {  "print",   test_print},
      {"asyncio", test_asyncio},
      {     NULL,         NULL},
  };

  for (struct test* test = tests; test->name != NULL; ++test) {
    printf("Running test '%s'... \n", test->name);
    test->rountine();
    printf("Passed!\n");
  }

  return 0;
}
