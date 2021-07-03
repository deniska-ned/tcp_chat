# No filename targets

.PHONY: all clean


# Compiler

CC := gcc


# Flags

OBJECT_FLAGS := -std=gnu99 -Wall # -Werror -Wpedantic -Wvla \
-Wextra -Wcast-align -Wcast-qual -Wconversion -Wenum-compare \
-Wfloat-equal -Wredundant-decls -Wsign-conversion

DEBUG_FLAGS := -g -O0


# Filenames

SRC_FILES := $(wildcard src/*.c)

RELEASE_OBJECT_FILES := $(SRC_FILES:src/%.c=out/release/%.o)
RELEASE_OBJECT_FILES_WITHOUT_MAIN := $(filter-out out/release/main.o, \
	$(RELEASE_OBJECT_FILES))
RELEASE_DEPENDENCE_FILES := $(SRC_FILES:src/%.c=out/release/%.d)

DEBUG_OBJECT_FILES := $(SRC_FILES:src/%.c=out/debug/%.o)
DEBUG_DEPENDENCE_FILES := $(SRC_FILES:src/%.c=out/debug/%.d)


# Targets

all: server.out debug_server.out


server.out: $(RELEASE_OBJECT_FILES)
	$(CC) -o $@ $^ -lm

debug_server.out: $(DEBUG_OBJECT_FILES)
	$(CC) -o $@ $^ -lm $(DEBUG_FLAGS) $(GCOV_FLAGS)


out/release/%.o: src/%.c
	@mkdir -p out/release
	$(CC) $(OBJECT_FLAGS) -o $@ -c $<

out/debug/%.o: src/%.c
	@mkdir -p out/debug/
	$(CC) $(OBJECT_FLAGS) -o $@ -c $< $(DEBUG_FLAGS) $(GCOV_FLAGS)


out/release/%.d: src/%.c
	@mkdir -p out/release
	$(CC) -MM $< | sed '1s|^|out/release/|' > $@

out/debug/%.d: src/%.c
	@mkdir -p out/debug/
	$(CC) -MM $< | sed '1s|^|out/debug/|' > $@


include $(RELEASE_DEPENDENCE_FILES)

include $(DEBUG_DEPENDENCE_FILES)


clean:
	rm -f server.out debug_server.out
	rm -rf out/
