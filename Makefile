# Makefile for building a single configuration of the virtual machine.
# Expects the following variables:
#
# MODE			"debug" or "release"

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -Wno-unused-parameter -Ideps/libuv/include/ -Itest/ -Lbuild -Lbuild/utf8proc `sdl2-config --cflags`

BUILD_TOP := build

ifeq ($(MODE), debug)
	CFLAGS += -O0 -DDEBUG -g
	BUILD_DIR := $(BUILD_TOP)/debug
else
	CFLAGS += -O3 -flto
	BUILD_DIR := $(BUILD_TOP)/release
endif

# Files.
HEADERS := $(wildcard src/*.h test/*.h)
SOURCES := $(wildcard src/*.c)
OBJECTS := $(addprefix $(BUILD_DIR)/, $(notdir $(SOURCES:.c=.o)))
LIBS := -luv_a -lutf8proc `sdl2-config --libs` -pthread -Wl,--no-as-needed -ldl

# Targets ---------------------------------------------------------------------

# Link the interpreter.
$(BUILD_DIR)/mochivm: $(OBJECTS) $(BUILD_TOP)/libuv_a.a $(BUILD_TOP)/utf8proc/libutf8proc.a
	@ printf "%8s %-40s %s %s\n" $(CC) $@ "$(CFLAGS)" "$(LIBS)"
	@ mkdir -p $(BUILD_DIR)
	@ $(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Compile object files.
$(BUILD_DIR)/%.o: src/%.c $(HEADERS)
	@ printf "%8s %-40s %s\n" $(CC) $< "$(CFLAGS)"
	@ mkdir -p $(BUILD_DIR)
	@ $(CC) -c $(C_LANG) $(CFLAGS) -o $@ $<

# Compile libuv
$(BUILD_TOP)/libuv_a.a:
	@ printf "Building libuv\n"
	@ mkdir -p $(BUILD_TOP)
	@ (cd $(BUILD_TOP) && cmake ../deps/libuv)
	@ cmake --build $(BUILD_TOP)

# Compile utf8proc
$(BUILD_TOP)/utf8proc/libutf8proc.a:
	@ printf "Building utf8proc\n"
	@ mkdir -p $(BUILD_TOP)/utf8proc
	@ (cd $(BUILD_TOP)/utf8proc && cmake ../../deps/utf8proc)
	@ (cd $(BUILD_TOP)/utf8proc && make)

# Format source files
format:
	@ printf "Formatting source files\n"
	@ clang-format -i $(HEADERS)
	@ clang-format -i $(SOURCES)

.PHONY: default

clean:
	@ rm -rf build