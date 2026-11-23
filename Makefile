
# @TODO Compilers to support gcc, clang, MSVC, TinyCC

ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

ifeq ($(OS),Windows_NT)
  SHELL := cmd.exe
  .SHELLFLAGS := /C
endif

ifeq ($(OS),Windows_NT)
  OUTPUT_EXE ?= $(ROOT)/bin/basin.exe
  OUTPUT_LIB ?= $(ROOT)/bin/basin.lib
  OUTPUT_DYN ?= $(ROOT)/bin/basin.dll
  OBJDIR ?= obj/windows
else
  OUTPUT_EXE ?= $(ROOT)/bin/basin
  OUTPUT_LIB ?= $(ROOT)/bin/libbasin.a
  OUTPUT_DYN ?= $(ROOT)/bin/libbasin.so
  OBJDIR ?= obj/linux
endif

BINDIR        ?= bin
CC            ?= gcc
CXX           ?= g++
AR            ?= ar
SILENT        ?=
ENABLE_TRACY  ?= 1


CFLAGS ?=
CFLAGS += -Wall -Werror -Wextra
CFLAGS += -Wno-unused-value -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-cast-function-type # @TODO Remove these and fix errors for final release?
CFLAGS += -fPIC # needed for shared library
CFLAGS += -I $(ROOT)/include -I $(ROOT)/src
CFLAGS += -g -DDEBUG_BUILD
# DEBUG_BUILD macro will use enums instead of 8-bit integers in some structs for debuggability.
# Ideally C has 8-bit enums but it does not. ISO C11 doesn't at least.
# CFLAGS += -O3
LFLAGS += -g

# Below is needed for debugging on windows with GDB (from mingw)
ifeq ($(OS),Windows_NT)
  LFLAGS += -lws2_32 -lwinmm -ldbghelp -static-libstdc++ -static-libgcc
endif

SRCDIRS := \
  $(ROOT)/src \
  $(ROOT)/src/basin \
  $(ROOT)/src/basin/backend \
  $(ROOT)/src/basin/core \
  $(ROOT)/src/basin/extra \
  $(ROOT)/src/basin/frontend \
  $(ROOT)/src/basin/util \
  $(ROOT)/src/platform \
  #

SRCFILES := $(foreach DIR,$(SRCDIRS),\
  $(wildcard $(DIR)/*.c) \
)

ifneq ($(ENABLE_TRACY),)
  SRCFILES += $(ROOT)/src/tracy/0.12.2/public/TracyClient.cpp
  CFLAGS   += -I $(ROOT)/src/tracy/0.12.2/public
endif

OBJFILES := $(patsubst $(ROOT)/src/%.c,$(OBJDIR)/%.o,$(SRCFILES))
OBJFILES := $(patsubst $(ROOT)/src/%.cpp,$(OBJDIR)/%.o,$(OBJFILES))
DEPFILES := $(patsubst %.o,%.d,$(OBJFILES))

.PHONY: all
# @TODO Compile libs again (for now it's unnecessary, i don't use them).
# all: $(OUTPUT_EXE) $(OUTPUT_LIB) $(OUTPUT_DYN)
all: $(OUTPUT_EXE)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
    -include $(DEP_FILES)
endif

ifeq ($(OS),Windows_NT)
  define makedirs =
    mkdir $(subst /,\,$(1)) 2>nul || exit /b 0
  endef
else
  define makedirs =
    mkdir -p $(1)
  endef
endif


$(OBJDIR) $(BINDIR):
	$(SILENT) $(call makedirs,$@)

VERSION    := 0.0.1-dev
CFLAGS += "-DBASIN_VERSION=\"$(VERSION)\""


ifeq ($(OS),Windows_NT)
  GIT_COMMIT := $(shell git rev-parse --short=10 HEAD 2>nul || echo unknown)
  BUILD_DATE := $(shell powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss'")
  CFLAGS += "-DBASIN_COMMIT=\"$(GIT_COMMIT)\"" "-DBASIN_DATE=\"$(BUILD_DATE)\""
else
  GIT_COMMIT := $(shell git rev-parse --short=10 HEAD 2>/dev/null || echo unknown)
  BUILD_DATE := $(shell date '+%Y-%m-%d %H:%M:%S')
  CFLAGS += "-DBASIN_COMMIT=\"$(GIT_COMMIT)\"" "-DBASIN_DATE=\"$(BUILD_DATE)\""
endif

$(OBJDIR)/%.o: $(ROOT)/src/%.c | $(OBJDIR)
	$(SILENT) $(call makedirs,$(@D))
	$(SILENT) $(CC) -c -MD $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: $(ROOT)/src/%.cpp | $(OBJDIR)
	$(SILENT) $(call makedirs,$(@D))
	$(SILENT) $(CXX) -c -MD $(CFLAGS) -o $@ $<

# @TODO For libraries we don't want to include main.o
# $(OUTPUT_LIB): $(OBJFILES) | $(BINDIR)
# 	$(SILENT) $(AR) rcs $@ $^

$(OUTPUT_EXE): $(OBJFILES) | $(BINDIR)
	$(SILENT) $(CXX) $(LFLAGS) -o $@ $^

# $(OUTPUT_DYN): $(OBJFILES) | $(BINDIR)
# 	$(SILENT) $(CXX) $(LFLAGS) -shared -o $@ $^


.PHONY: clean
clean:
ifeq ($(OS),Windows_NT)
	rd /S /Q obj bin 2>nul || exit /b 0
else
	rm -rf obj bin
endif
