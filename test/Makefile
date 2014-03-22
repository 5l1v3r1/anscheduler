CFILES=$(wildcard env/*.c)
CFLAGS=-Wall -I../include -I./include -I../lib/anidxset/src -I../lib/anlock/src
TEST_PROGS=test_single.c
BUILD_FILES=../lib/anlock/build/*.o ../lib/anidxset/build/*.o ../build/*.o env/build/*.o

tests: testLib build env/build
	for file in $(TEST_PROGS); do \
		gcc $(CFLAGS) $$file $(BUILD_FILES) -o build/`basename $$file .c`; \
	done

testLib: ansched_lib anidxset_lib anlock_lib env/build
	for file in $(CFILES); do \
		gcc $(CFLAGS) -c $$file -o env/build/`basename $$file .c`.o; \
	done

ansched_lib:
	cd .. && make INCLUDES=-I./test/include CFLAGS=-Wall

anidxset_lib:
	cd ../lib/anidxset && make

anlock_lib:
	cd ../lib/anlock && make

env/build:
	mkdir env/build

build:
	mkdir build

clean:
	cd .. && make clean
	cd ../lib/anlock && make clean
	cd ../lib/anidxset && make clean
	rm -rf build/
	rm -rf env/build
