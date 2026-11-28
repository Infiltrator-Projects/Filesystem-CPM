# SPDX-License-Identifier: GPL-3.0-or-later
CC ?= cc
CFLAGS ?= -O2
BASE = -std=c11 -Wall -Wextra -Werror -I Libraries/include
SRC = native/filesystems/cpm/cpm.c
.PHONY: all check clean
all: build/CPM.so build/freestanding/CPM.so
build/CPM.so: $(SRC) Makefile
	mkdir -p build
	$(CC) $(BASE) $(CFLAGS) -fPIC -fvisibility=hidden -shared -Wl,-z,defs -o $@ $(SRC)
build/freestanding/CPM.so: $(SRC) Makefile
	mkdir -p build/freestanding
	$(CC) $(BASE) -O2 -march=x86-64 -DIFS_FREESTANDING -ffreestanding -fno-builtin -fno-stack-protector -fno-tree-loop-distribute-patterns -fPIC -fvisibility=hidden -shared -nostdlib -Wl,-z,defs,-Bsymbolic -o $@ $(SRC)
build/test_cpm: tests/test_cpm.c
	mkdir -p build
	$(CC) $(BASE) $(CFLAGS) -o $@ $< -ldl
check: all build/test_cpm
	test "$$(nm -D --defined-only build/CPM.so | awk '{print $$3}')" = infiltrator_filesystem_api_v2
	test "$$(nm -D --defined-only build/freestanding/CPM.so | awk '{print $$3}')" = infiltrator_filesystem_api_v2
	test -z "$$(nm -D --undefined-only build/freestanding/CPM.so)"
	! readelf -d build/freestanding/CPM.so | grep -q NEEDED
	build/test_cpm ./build/CPM.so
	build/test_cpm ./build/freestanding/CPM.so
clean:
	rm -rf build
