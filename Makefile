FILENAME := example-cli
PLATFORM := $(shell uname -s | tr A-Z a-z)_$(shell arch)
COMPILER_PREFIX :=
EXE := $(FILENAME)_$(PLATFORM)

include Makefile.base
