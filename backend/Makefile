CC ?= gcc
AR ?= ar
CFLAGS = -Wall -Wextra -O2 -g -std=c11 -D_GNU_SOURCE -I./src

TARGET = build/libsystem_monitor_backend.a
SRCDIR = src
BUILDDIR = build

SOURCES = \
	$(SRCDIR)/proc_parser.c \
	$(SRCDIR)/history.c

OBJECTS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean
