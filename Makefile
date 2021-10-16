# Makefile for building a single configuration of the virtual machine.
# Expects the following variables:
#
# MODE			"debug" or "release"

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -Wno-unused-parameter -Ideps/libuv/include/ -Itest/ -Lbuild

ifeq ($(MODE), debug)
	CFLAGS += -O0 -DDEBUG -g
	BUILD_DIR := build/debug
else
	CFLAGS += -O3 -flto
	BUILD_DIR := build/release
endif

# Files.
HEADERS := $(wildcard src/*.h test/*.h)
SOURCES := $(wildcard src/*.c)
OBJECTS := $(addprefix $(BUILD_DIR)/zhenzhu/, $(notdir $(SOURCES:.c=.o)))
LIBS := -luv_a -pthread -Wl,--no-as-needed -ldl

# Targets ---------------------------------------------------------------------

# Link the interpreter.
build/zhenzhu: $(OBJECTS)
	@ printf "%8s %-40s %s %s\n" $(CC) $@ "$(CFLAGS)" "$(LIBS)"
	@ mkdir -p build
	@ $(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Compile object files.
$(BUILD_DIR)/zhenzhu/%.o: src/%.c $(HEADERS)
	@ printf "%8s %-40s %s\n" $(CC) $< "$(CFLAGS)"
	@ mkdir -p $(BUILD_DIR)/zhenzhu
	@ $(CC) -c $(C_LANG) $(CFLAGS) -o $@ $<

.PHONY: default

clean:
	@ rm -rf build