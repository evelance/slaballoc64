# slaballoc64

## About

This is an experimental single-threaded memory allocator for small and uniform objects. It uses
mmap() to allocate a linked list of pages and can track up to 64 objects per page. It is inspired by
Jeff Bonwick's slab allocator.

For 64-byte objects it seems to be quite fast and uses less memory compared to the default glibc malloc (800MiB vs 950MiB).

Compiled with `g++ 9.2.1 (20191126)` and `Debian GLIBC 2.29-3`

```
g++ -std=c++11 -ggdb -Wall -Wextra -pedantic -O3 -m64 -o test_no_address_sanitize.out test.cpp
g++ -std=c++11 -ggdb -Wall -Wextra -pedantic -O3 -m64 -fsanitize=address -fno-omit-frame-pointer -o test_with_address_sanitize.out test.cpp
./test_no_address_sanitize.out
Generating random indices...
slaballoc64: 2x random allocation/free of 10.362.880 objects of size 64
Round 1 of 5 -   3.206,82ms
Round 2 of 5 -   2.743,29ms
Round 3 of 5 -   2.742,73ms
Round 4 of 5 -   2.732,84ms
Round 5 of 5 -   2.752,53ms
Default malloc: 2x random allocation/free of 10.362.880 objects of size 64
Round 1 of 5 -   6.131,85ms
Round 2 of 5 -   3.654,41ms
Round 3 of 5 -   5.957,50ms
Round 4 of 5 -   3.803,11ms
Round 5 of 5 -   5.907,50ms
./test_with_address_sanitize.out
Generating random indices...
slaballoc64: 2x random allocation/free of 10.362.880 objects of size 64
Round 1 of 5 -   4.031,14ms
Round 2 of 5 -   5.600,02ms
Round 3 of 5 -   3.395,71ms
Round 4 of 5 -   5.729,61ms
Round 5 of 5 -   3.385,66ms
Default malloc: 2x random allocation/free of 10.362.880 objects of size 64
Round 1 of 5 -  15.601,02ms
Round 2 of 5 -  14.080,08ms
Round 3 of 5 -  15.123,90ms
Round 4 of 5 -  11.377,98ms
Round 5 of 5 -  13.512,61ms
```
