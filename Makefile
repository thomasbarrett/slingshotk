CFLAGS=-Wall -std=c11 -Iinclude -g -fsanitize=address 

all: bin/main

bin:
	@mkdir bin

obj:
	@mkdir obj

bin/main: main/main.c obj/bitset.o obj/array.o obj/util.o obj/blob.o | bin
	@$(CC) $(CFLAGS) $^ -o $@

obj/bitset.o: src/bitset.c | include/bitset.h obj
	@$(CC) $(CFLAGS) $^ -c -o $@

obj/array.o: src/array.c | include/array.h obj
	@$(CC) $(CFLAGS) $^ -c -o $@

obj/util.o: src/util.c | include/util.h obj
	@$(CC) $(CFLAGS) $^ -c -o $@

obj/blob.o: src/blob.c | include/blob.h obj
	@$(CC) $(CFLAGS) $^ -c -o $@

.PHONY: clean
clean:
	@rm bin/*
	@rm obj/*
