FILENAME := 3at

ifdef TARGET
  EXE := $(FILENAME)_$(TARGET)
  BUILDTARGET := $(TARGET)
else
  EXE := $(FILENAME)
  BUILDTARGET := native
endif

JOBS := $(shell nproc)
OUTPUT := build/$(EXE)

.DEFAULT_GOAL := all

CC ?= gcc
CXX ?= g++
STRIP ?= strip
PYTHON ?= python3
ZIG_VENV ?= .venv
SYSTEM_ZIG := $(shell command -v zig 2>/dev/null)
ZIG ?= $(if $(SYSTEM_ZIG),$(SYSTEM_ZIG),$(abspath $(ZIG_VENV))/bin/python-zig)

ifeq ($(NDEBUG),1)
  OPT := -Os -flto -ffunction-sections -fdata-sections
  MODE := release
  LINK_PLATFORM := $(if $(TARGET),$(TARGET),$(shell uname -s))
  ifneq ($(filter Darwin %macos,$(LINK_PLATFORM)),)
    LDFLAGS += -Wl,-dead_strip -Wl,-S -Wl,-x
  else
    OPT += -static
    LDFLAGS += -Wl,--gc-sections -Wl,--strip-all
  endif
else
  OPT := -O0 -ggdb -ftrapv
  MODE := debug
endif

ifeq ($(SANITIZE),1)
  OPT += -fsanitize=address,undefined
  MODE := $(MODE)-sanitized
endif

CFLAGS = $(OPT) -Wall -pthread
CXXFLAGS = $(OPT) -Wall -std=c++17 -pthread
CPPFLAGS ?= -MMD -MP

SRCS_C := $(wildcard src/*.c)
SRCS_CXX := $(wildcard src/*.cpp)
BUILDDIR := build/obj/$(BUILDTARGET)/$(MODE)
MODE_OUTPUT := $(BUILDDIR)/$(EXE)
OBJS := $(SRCS_C:src/%.c=$(BUILDDIR)/%.c.o)
OBJS += $(SRCS_CXX:src/%.cpp=$(BUILDDIR)/%.cpp.o)
DEPS := $(OBJS:.o=.d)

TESTDIR := build/tests/$(MODE)
CODEC_TEST := $(TESTDIR)/sdx2_quality_test
PARALLEL_TEST := $(TESTDIR)/parallel_test
SCORE_TEST := $(TESTDIR)/encoder_score_test
CODEC_TEST_SRCS := tests/sdx2_quality_test.c \
                   src/adpcm-dns.c src/adpcm-lib.c src/clamp.c \
                   src/dpcm-xq-dns.c src/dpcm-xq.c \
                   src/sdx2_decode.c src/sdx2_encode.c
CODEC_ALLOC_FLAGS := -Dmalloc=codec_test_malloc -Dfree=codec_test_free

all: $(OUTPUT) build/license.txt

help:
	@echo "Targets: all, test, zig-venv, release, strip, install, clean"
	@echo "  make                    Build native debug binary"
	@echo "  make test               Run C and C++ regression suites"
	@echo "  make NDEBUG=1            Build optimized native binary"
	@echo "  make SANITIZE=1 test     Run tests with ASan and UBSan"
	@echo "  make zig-venv            Use Zig on PATH or install Zig 0.16.0 in .venv"
	@echo "  make release             Cross-compile Linux, Windows, and macOS binaries"
	@echo "  make install             Install to DESTDIR/PREFIX/bin"

$(MODE_OUTPUT): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) -lm

$(OUTPUT): $(MODE_OUTPUT) FORCE | build
	@cmp -s $< $@ || cp $< $@

build/license.txt: license.txt | build
	cp $< $@

$(BUILDDIR)/%.c.o: src/%.c | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.cpp.o: src/%.cpp | $(BUILDDIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

test: | $(TESTDIR)
	$(CC) $(CFLAGS) $(CODEC_ALLOC_FLAGS) -Isrc $(CODEC_TEST_SRCS) -lm -o $(CODEC_TEST)
	$(CODEC_TEST)
	$(CXX) $(CXXFLAGS) -Isrc tests/parallel_test.cpp -o $(PARALLEL_TEST)
	$(PARALLEL_TEST)
	$(CXX) $(CXXFLAGS) -Isrc tests/encoder_score_test.cpp src/encoder_score.cpp -o $(SCORE_TEST)
	$(SCORE_TEST)

build $(BUILDDIR) $(TESTDIR):
	mkdir -p $@

strip: $(OUTPUT)
	$(STRIP) --strip-all $(MODE_OUTPUT)
	cp $(MODE_OUTPUT) $<

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
install: $(OUTPUT)
	install -Dm755 $< $(DESTDIR)$(BINDIR)/$(EXE)

zig-venv:
ifneq ($(SYSTEM_ZIG),)
	@echo "Using system Zig: $(SYSTEM_ZIG)"
else
	$(PYTHON) -m venv "$(ZIG_VENV)"
	"$(ZIG_VENV)/bin/python" -m pip install "ziglang==0.16.0"
endif

release:
	@"$(ZIG)" version >/dev/null 2>&1 || { \
		echo "Zig not found; run 'make zig-venv' first." >&2; \
		exit 1; \
	}
	$(MAKE) NDEBUG=1 -j$(JOBS) \
		CC="$(ZIG) cc -target x86_64-linux-musl" \
		CXX="$(ZIG) c++ -target x86_64-linux-musl" \
		STRIP="$(ZIG) llvm-strip" TARGET="x86_64-linux-musl" \
		OPT="-Oz -flto -ffunction-sections -fdata-sections -static"
	$(MAKE) NDEBUG=1 -j$(JOBS) \
		CC="$(ZIG) cc -target aarch64-linux-musl" \
		CXX="$(ZIG) c++ -target aarch64-linux-musl" \
		STRIP="$(ZIG) llvm-strip" TARGET="aarch64-linux-musl" \
		OPT="-Oz -flto -ffunction-sections -fdata-sections -static"
	$(MAKE) NDEBUG=1 -j$(JOBS) \
		CC="$(ZIG) cc -target x86-windows-gnu" \
		CXX="$(ZIG) c++ -target x86-windows-gnu" \
		STRIP="$(ZIG) llvm-strip" TARGET="x86-windows-gnu.exe" \
		OPT="-Oz -ffunction-sections -fdata-sections -static"
	$(MAKE) NDEBUG=1 -j$(JOBS) \
		CC="$(ZIG) cc -target x86_64-windows-gnu" \
		CXX="$(ZIG) c++ -target x86_64-windows-gnu" \
		STRIP="$(ZIG) llvm-strip" TARGET="x86_64-windows-gnu.exe" \
		OPT="-Oz -ffunction-sections -fdata-sections -static"
	$(MAKE) NDEBUG=1 -j$(JOBS) \
		CC="$(ZIG) cc -target aarch64-macos" \
		CXX="$(ZIG) c++ -target aarch64-macos" \
		STRIP="$(ZIG) llvm-strip" TARGET="aarch64-macos" \
		OPT="-Oz -ffunction-sections -fdata-sections"

clean:
	rm -rfv build/

FORCE:

.PHONY: all help test strip install zig-venv release clean FORCE

-include $(DEPS)
