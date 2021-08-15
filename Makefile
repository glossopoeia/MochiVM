# Makefile for building a single configuration of the virtual machine.
# Expects the following variables:
#
# MODE			"debug" or "release"

CFLAGS := -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter

ifeq ($(MODE), debug)
	CFLAGS += -O0 -DDEBUG -g
	BUILD_DIR := build/debug
else
	CFLAGS += -O3 -flto
	BUILD_DIR := build/release
endif

# Files.
HEADERS := $(wildcard src/*.h)
SOURCES := $(wildcard src/*.c)
OBJECTS := $(addprefix $(BUILD_DIR)/zhenzhu/, $(notdir $(SOURCES:.c=.o)))

# Targets ---------------------------------------------------------------------

# Link the interpreter.
build/zhenzhu: $(OBJECTS)
	@ printf "%8s %-40s %s\n" $(CC) $@ "$(CFLAGS)"
	@ mkdir -p build
	@ $(CC) $(CFLAGS) $^ -o $@

# Compile object files.
$(BUILD_DIR)/zhenzhu/%.o: src/%.c $(HEADERS)
	@ printf "%8s %-40s %s\n" $(CC) $< "$(CFLAGS)"
	@ mkdir -p $(BUILD_DIR)/zhenzhu
	@ $(CC) -c $(C_LANG) $(CFLAGS) -o $@ $<

.PHONY: default

clean:
	@ rm -rf build