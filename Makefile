PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra

.PHONY: all install uninstall clean

all: copy

copy: src/copy.c
	$(CC) $(CFLAGS) -o copy src/copy.c

install: copy
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 copy $(DESTDIR)$(BINDIR)/copy
	install -d $(DESTDIR)$(MANDIR)
	install -m 644 man/copy.1 $(DESTDIR)$(MANDIR)/copy.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/copy
	rm -f $(DESTDIR)$(MANDIR)/copy.1

clean:
	rm -f copy
