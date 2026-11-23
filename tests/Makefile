
# @TODO Compilers to support gcc, clang, MSVC, TinyCC

ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))..)
TEST_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))


ifeq ($(OS),Windows_NT)
  SHELL := cmd.exe
  .SHELLFLAGS := /C
endif

ifeq ($(OS),Windows_NT)
  OBJDIR ?= obj/windows
  PATH := $(ROOT)/bin;$(PATH)
  export PATH
else
  OBJDIR ?= obj/linux
  PATH := $(ROOT)/bin:$(PATH)
  export PATH
endif


TEST_DIRS ?= \
    $(TEST_ROOT)/arithmetic \
    $(TEST_ROOT)/control_flow

TEST_FILES ?= $(foreach DIR,$(TEST_DIRS),\
    $(wildcard $(DIR)/*.bsn)\
)

TEST_EXES := $(patsubst $(TEST_ROOT)/%.bsn,$(OBJDIR)/%.exe,$(TEST_FILES))

BASIN         ?= basin
CC            ?= gcc
SILENT        ?= 

CFLAGS := -g
LFLAGS := -g


.PHONY: test
test: $(TEST_EXES)
	for item in $(TEST_EXES); do echo "RUNNING $item"; done


ifeq ($(OS),Windows_NT)
  define makedirs =
    mkdir $(subst /,\,$(1)) 2>nul || exit /b 0
  endef
else
  define makedirs =
    mkdir -p $(1)
  endef
endif

$(OBJDIR)/testutil.o: $(TEST_ROOT)/testutil.c | $(OBJDIR)
	$(SILENT) $(CC) -c -MD $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: $(TEST_ROOT)/%.bsn | $(OBJDIR)
	$(SILENT) $(call makedirs,$(@D))
	$(SILENT) $(BASIN) -o $@ $<

$(OBJDIR)/%.exe: $(OBJDIR)/%.o $(OBJDIR)/testutil.o | $(OBJDIR)
	$(SILENT) $(call makedirs,$(@D))
	$(SILENT) $(CC) $(LFLAGS) -o $@ $^

$(OBJDIR):
	$(SILENT) $(call makedirs,$@)


.PHONY: clean
clean:
ifeq ($(OS),Windows_NT)
	rd /S /Q obj bin 2>nul || exit /b 0
else
	rm -rf obj bin
endif
