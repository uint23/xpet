# tools
CC ?= cc
PKG_CONFIG ?= pkg-config

# install dirs
PREFIX ?= /usr/local
DESTDIR ?=
BIN := xpet
MAN := xpet.1
MAN_DIR := $(PREFIX)/share/man/man1

# layout
SRC_DIR := ./
OBJ_DIR := obj/

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

# flags
CPPFLAGS ?= -Isrc -D_FORTIFY_SOURCE=2

# compile flags + warnings, hardening
CFLAGS ?= -std=c99 -Os -pipe \
          -Wall -Wextra -Wformat=2 -Werror=format-security \
          -Wshadow -Wpointer-arith -Wcast-qual -Wwrite-strings \
          -Wmissing-prototypes -Wstrict-prototypes -Wswitch-enum \
          -Wundef -Wvla -fno-common -fno-strict-aliasing \
          -fstack-protector-strong -fPIE

# linker
LDFLAGS ?= -Wl,-Os -pie

# libraries
CFLAGS += $(shell $(PKG_CONFIG) --cflags x11) -lm
LDLIBS += $(shell $(PKG_CONFIG) --libs   x11) -lm

.PHONY: all clean install uninstall clangd
.SUFFIXES:

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEP)

$(OBJ_DIR):
	@mkdir -p $@

clean:
	@rm -rf $(OBJ_DIR) $(BIN)

install: all
	@echo "installing $(BIN) to $(DESTDIR)$(PREFIX)/bin..."
	@mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	@install -m 755 $(BIN) "$(DESTDIR)$(PREFIX)/bin/$(BIN)"
	@echo "installing man page to $(DESTDIR)$(MAN_DIR)..."
	@mkdir -p "$(DESTDIR)$(MAN_DIR)"
	@install -m 644 $(MAN) "$(DESTDIR)$(MAN_DIR)/"
	@echo "installation complete :)"

uninstall:
	@echo "uninstalling $(BIN) from $(DESTDIR)$(PREFIX)/bin..."
	@rm -f "$(DESTDIR)$(PREFIX)/bin/$(BIN)"
	@echo "uninstalling man page from $(DESTDIR)$(MAN_DIR)..."
	@rm -f "$(DESTDIR)$(MAN_DIR)/$(MAN)"
	@echo "uninstallation complete :)"

# dev tools
clangd:
	@echo "generating compile_flags.txt"
	@rm -f compile_flags.txt
	@for flag in $(CPPFLAGS) $(CFLAGS); do echo $$flag >> compile_flags.txt; done
