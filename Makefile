.PHONY: all debug clean

CC = clang
VERBOSE ?= @
OBJDIR ?= ./build
BINDIR ?= ./bin
DEPDIR ?= ./dep

WARNFLAGS = -Wall -pedantic -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable -Wno-zero-length-array
OPT ?= -O3
OPTFLAGS = $(OPT) -march=native
CCFLAGS = $(OPTFLAGS) $(WARNFLAGS) -g -pthread

C_SOURCES = $(shell find . -name "*.c" -and ! -name '.*' )
# C_OBJECTS = $(notdir $(C_SOURCES:.c=.o))
BINARIES = $(notdir $(C_SOURCES:.c=))
DEP_FILES = $(patsubst ./%.c,$(DEPDIR)/%.d,$(C_SOURCES))
OBJPRE = $(addprefix $(OBJDIR)/, $(C_OBJECTS))
BINPRE = $(addprefix $(BINDIR)/, $(BINARIES))

all: $(BINPRE) $(MAKEFILE_LIST)

debug:
	@ $(MAKE) OPT=-O0 BINDIR=./debug DEPDIR=./debug-dep

# $(DEPDIR)/%.d : %.c $(MAKEFILE_LIST)
# 	@echo "DEP		$@"
# 	@if test \( ! \( -d $(@D) \) \) ;then mkdir -p $(@D);fi
# 	$(VERBOSE) $(CXX) $(CXXFLAGS) -MM -MT $(OBJDIR)/$*.o -MF $@ $<

$(DEPDIR)/%.d : %.c $(MAKEFILE_LIST)
	@echo "DEP		$@"
	@if test \( ! \( -d $(@D) \) \) ;then mkdir -p $(@D);fi
	$(VERBOSE) $(CXX) $(CXXFLAGS) -MM -MT $(BINDIR)/$* -MF $@ $<

# $(OBJDIR)/%.o : %.c $(MAKEFILE_LIST)
# 	@echo "CC		$@"
# 	@if test \( ! \( -d $(@D) \) \) ;then mkdir -p $(@D);fi
# 	$(VERBOSE) $(CC) -c $(CCFLAGS) -o $@ $<

$(BINDIR)/% : %.c $(MAKEFILE_LIST)
	@echo "CCLD		$@"
	@if test \( ! \( -d $(@D) \) \) ;then mkdir -p $(@D);fi
	$(VERBOSE) $(CC) $(CCFLAGS) -o $@ $<

clean:
	@echo "RM		$(BINDIR)"
	$(VERBOSE) rm -rf $(BINDIR)
	@echo "RM		$(OBJDIR)"
	$(VERBOSE) rm -rf $(OBJDIR)
	@echo "RM		$(DEPDIR)"
	$(VERBOSE) rm -rf $(DEPDIR)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEP_FILES)
endif
