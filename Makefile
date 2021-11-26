# -----------------------------
# Make settings
# -----------------------------

MAKEFLAGS=

# -----------------------------
# Compiler settings
# -----------------------------

CC  = gcc
LD  = gcc
RM  = rm

# -----------------------------
# Project settings
# -----------------------------

PROJNAME         = japCrosswords
PROJNAME_DEBUG   = $(PROJNAME)_debug
PROJNAME_RELEASE = $(PROJNAME)

# -----------------------------
# Compiler flags
# -----------------------------

WFLAGS  = -Wall -Wextra -pedantic -Wdouble-promotion -Wformat=2 -Winit-self \
          -Wmissing-include-dirs -Wswitch-default -Wswitch-enum -Wunused-local-typedefs \
          -Wunused -Wuninitialized -Wsuggest-attribute=pure \
          -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wfloat-equal \
          -Wundef -Wshadow \
          -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings \
          -Wconversion -Wlogical-op \
          -Wmissing-field-initializers \
          -Wmissing-format-attribute -Wpacked -Winline -Wredundant-decls \
          -Wvector-operation-performance -Wdisabled-optimization \
          -Wstack-protector
#              -Wstrict-overflow=5 -Wunsafe-loop-optimizations
               
#CWFLAGS      = -Wdeclaration-after-statement -Wc++-compat -Wbad-function-cast \
#               -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -Wunsuffixed-float-constants
#-Wzero-as-null-pointer-constant -Wpadded
DEBUGFLAGS   = -g3 -O0
RELEASEFLAGS = -g0 -O3
CFLAGS       = -std=c11

# -----------------------------
# Linker flags
# -----------------------------

LIBS   = pthread

# -----------------------------
# Some automatic stuff
# -----------------------------

LFLAGS += $(foreach lib,$(LIBS),-l$(lib)) `sdl-config --cflags --libs`

SRCFILES := $(wildcard ./*.c)
OBJFILES := $(patsubst %.c,%.o,$(SRCFILES))
DEPFILES := $(patsubst %.c,%.d,$(SRCFILES))

OBJFILES_DEBUG = $(patsubst %.o,debug/%.o,$(OBJFILES))
OBJFILES_RELEASE = $(patsubst %.o,release/%.o,$(OBJFILES))

DEPFILES_DEBUG = $(patsubst %.d,debug/%.d,$(DEPFILES))
DEPFILES_RELEASE = $(patsubst %.d,release/%.d,$(DEPFILES))

# -----------------------------
# Make targets
# -----------------------------

.PHONY: all debug relase clean

all: $(PROJNAME_DEBUG) $(PROJNAME_RELEASE) Makefile
	@echo "  [ finished ]"

debug: $(PROJNAME_DEBUG) Makefile
	@echo "  [ done ]"

release: $(PROJNAME_RELEASE) Makefile
	@echo "  [ done ]"
	
cleanall: clean

#-include $(DEPFILES_DEBUG)
#-include $(DEPFILES_RELEASE)

$(PROJNAME_DEBUG): $(OBJFILES_DEBUG) $(SRCFILES) Makefile
	@echo "  [ Linking $@ ]" && \
	$(LD) $(OBJFILES_DEBUG) -o $@ $(LFLAGS)

$(PROJNAME_RELEASE): $(OBJFILES_RELEASE) $(SRCFILES) Makefile
	@echo "  [ Linking $@ ]" && \
	$(LD) $(OBJFILES_RELEASE) -o $@ $(LFLAGS)

debug/%.o: %.c Makefile
	@echo "  [ Compiling $< ]" && \
	mkdir debug/$(dir $<) -p && \
	$(CC) $(CFLAGS) $(WFLAGS) $(CWFLAGS) $(DEBUGFLAGS) -MMD -MP -c $< -o $@
	
release/%.o: %.c Makefile
	@echo "  [ Compiling $< ]" && \
	mkdir release/$(dir $<) -p && \
	$(CC) $(CFLAGS) $(WFLAGS) $(CWFLAGS) $(RELEASEFLAGS) -MMD -MP -c $< -o $@
	
clean:
	-@$(RM) -f $(wildcard $(OBJFILES_DEBUG) $(OBJFILES_RELEASE) $(DEPFILES_DEBUG) $(DEPFILES_RELEASE) $(PROJNAME_DEBUG) $(PROJNAME_RELEASE)) && \
	$(RM) -rfv debug release && \
	$(RM) -rfv `find ./ -name "*~"` && \
	echo "  [ clean main done ]"

