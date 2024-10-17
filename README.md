# cgc
A mark and sweep garbage collector written in c

This garbage collector only works on UNIX based operating systems and does not work on windows due to the absence of the `<mmap.h>` header.

## Compile and run tests
```
make test
```

## Usage
Compile your program with the src and include files of cgc. Then use cgc as follows:

```
#include "gc.h"

. . .

int main (int argc, char **argv) {
  alloc_t *allocator = alloc_init();
  if (allocator == NULL) exit(-1);

  . . .

  for (int i = 0; i < 1000; i++) {
    int *my_obj = mem_alloc(allocator, sizeof(int));
  }

  . . .

  return 0;
}

```
