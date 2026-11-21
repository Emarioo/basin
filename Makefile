
# @TODO Makefile compatible with Windows

# @TODO Compilers to support gcc, clang, MSVC, TinyCC

ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

OUTPUT_EXE ?= $(ROOT)/bin/basin
OUTPUT_LIB ?= $(ROOT)/bin/libbasin.a
OUTPUT_DYN ?= $(ROOT)/bin/libbasin.so


BINDIR        ?= bin
CC            ?= gcc
AR            ?= ar
SILENT        ?= @
ENABLE_TRACY  ?= 1

# obj/linux or obj/windows
OBJDIR ?= obj

CFLAGS ?=
CFLAGS += -Wall -Werror -Wextra
CFLAGS += -Wno-unused-value -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-cast-function-type # @TODO Remove these and fix errors for final release?
CFLAGS += -fPIC # needed for shared library
CFLAGS += -I $(ROOT)/include -I $(ROOT)/src
CFLAGS += -g
# CFLAGS += -O3
LFLAGS +=


# @TODO We may need these
# if platform.system() == "Windows":
# 	LDFLAGS += " -lws2_32 -lwinmm -ldbghelp -static-libstdc++ -static-libgcc"


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

.PHONY: all
all: $(OUTPUT_EXE) $(OUTPUT_LIB) $(OUTPUT_DYN)

$(OBJDIR):
	$(SILENT) mkdir -p $@

$(BINDIR):
	$(SILENT) mkdir -p $@

VERSION    := 0.0.1-dev
CFLAGS += "-DBASIN_VERSION=\"$(VERSION)\""

# @TODO Only add these if windows or linux
GIT_COMMIT := $(shell git rev-parse --short=10 HEAD 2>/dev/null || echo unknown)
BUILD_DATE := $(shell date '+%Y-%m-%d %H:%M:%S')
CFLAGS += "-DBASIN_COMMIT=\"$(GIT_COMMIT)\"" "-DBASIN_DATE=\"$(BUILD_DATE)\""

$(OBJDIR)/%.o: $(ROOT)/src/%.c | $(OBJDIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(CC) -c -MD $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: $(ROOT)/src/%.cpp | $(OBJDIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(CXX) -c -MD $(CFLAGS) -o $@ $<

$(OUTPUT_LIB): $(OBJFILES) | $(BINDIR)
	$(SILENT) $(AR) rcs $@ $^

$(OUTPUT_EXE): $(OBJFILES) | $(BINDIR)
	$(SILENT) $(CC) $(LFLAGS) -o $@ $^

$(OUTPUT_DYN): $(OBJFILES) | $(BINDIR)
	$(SILENT) $(CC) $(LFLAGS) -shared -o $@ $^


.PHONY: clean
clean:
	rm -rf obj bin
