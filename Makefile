V =

ifeq ($(strip $(V)),)
	E = @echo
	Q = @
else
	E = @\#
	Q =
endif
export E Q

PROGRAM = emacs

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

SRC = basic.c bind.c buffer.c display.c eval.c exec.c file.c fileio.c \
	globals.c input.c isearch.c line.c lock.c main.c names.c \
	undo.c edit.c help.c \
	posix.c random.c region.c search.c spawn.c spell.c tcap.c \
	usage.c utf8.c version.c window.c word.c wrapper.c

OBJ = $(SRC:.c=.o)

HDR = ebind.h efunc.h epath.h estruct.h evar.h globals.h line.h usage.h \
	utf8.h util.h version.h wrapper.h

CC ?= cc
WARNINGS = -Wall -Wstrict-prototypes -Wno-unused-result
DEFINES = -DPOSIX -D_GNU_SOURCE
OPTIMIZE ?= -O2 -pipe
LTO := $(shell echo 'int main(void){return 0;}' | $(CC) -x c -flto=auto - -o /dev/null 2>/dev/null && echo -flto=auto || (echo 'int main(void){return 0;}' | $(CC) -x c -flto - -o /dev/null 2>/dev/null && echo -flto))
NATIVE ?= 0
ifeq ($(NATIVE),1)
OPTIMIZE += -march=native
endif
CFLAGS ?= $(OPTIMIZE)
STD = -std=gnu11

LIBS = ncurses
HUNSPELL ?= 0
ifeq ($(HUNSPELL),1)
HAVE_HUNSPELL := $(shell pkg-config --exists hunspell && echo 1)
ifeq ($(HAVE_HUNSPELL),1)
LIBS += hunspell
DEFINES += -DHUNSPELL
else
$(warning hunspell not installed, not building with spell checking)
endif
endif

PKG_CONFIG_CFLAGS = $(shell pkg-config --cflags $(LIBS) 2>/dev/null)
PKG_CONFIG_LIBS = $(shell pkg-config --libs $(LIBS) 2>/dev/null || echo -lncurses)
ALL_CFLAGS = $(STD) $(CFLAGS) $(LTO) $(WARNINGS) $(DEFINES) $(PKG_CONFIG_CFLAGS)
LDLIBS += $(PKG_CONFIG_LIBS)
BUILD_FLAGS = $(ALL_CFLAGS) $(LDLIBS)

all: $(PROGRAM)

$(PROGRAM): $(OBJ)
	$(E) "  LINK    " $@
	$(Q) $(CC) $(CFLAGS) $(LTO) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

.c.o:
	$(E) "  CC      " $@
	$(Q) $(CC) $(ALL_CFLAGS) -c $<

$(OBJ): $(HDR)

.build-flags: FORCE
	$(Q) echo '$(BUILD_FLAGS)' | cmp -s - $@ || echo '$(BUILD_FLAGS)' > $@

FORCE:
.PHONY: FORCE all clean install uninstall

$(PROGRAM) $(OBJ): .build-flags

clean:
	$(E) "  CLEAN"
	$(Q) rm -f $(PROGRAM) em core *.o .build-flags

install: $(PROGRAM)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(PROGRAM) $(DESTDIR)$(BINDIR)/emacs
	@echo
	@echo "Installed $(DESTDIR)$(BINDIR)/emacs"
	@case ":$$PATH:" in *":$(BINDIR):"*) ;; *) echo "NOTE: $(BINDIR) is not in your PATH - add it to use 'emacs'.";; esac
	@echo "If your shell still starts another emacs, run: hash -r   (or open a new terminal)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/emacs
