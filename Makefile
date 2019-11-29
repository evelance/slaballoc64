# Build and run test
CFLAGS    := -std=c++11 -ggdb -Wall -Wextra -pedantic -O3 -m64
CFLAGS_AS := -fsanitize=address -fno-omit-frame-pointer

all: test_no_address_sanitize.out test_with_address_sanitize.out run-tests

test_no_address_sanitize.out: slaballoc64.hpp test.cpp Makefile
	$(CXX) $(CFLAGS) -o $@ test.cpp

test_with_address_sanitize.out: slaballoc64.hpp test.cpp Makefile
	$(CXX) $(CFLAGS) $(CFLAGS_AS) -o $@ test.cpp

run-tests: test_no_address_sanitize.out test_with_address_sanitize.out
	./test_no_address_sanitize.out
	./test_with_address_sanitize.out
