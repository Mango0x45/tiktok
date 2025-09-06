CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -std=c23 \
	-I$$(brew --prefix gettext)/include \
	-L$$(brew --prefix gettext)/lib \
	-lintl

PREFIX = /usr/local
DPREFIX = $(DESTDIR)$(PREFIX)
PODIR = $(DPREFIX)/share/locale

all: tiktok

tiktok: main.c
	$(CC) $(CFLAGS) -DPODIR='"$(PODIR)"' -o $@ $<

extract:
	xgettext --from-code=UTF-8 -k_ -o po/tiktok.pot main.c
	find po -name '*.po' -exec msgmerge {} po/tiktok.pot -o {} \;

translations:
	find po -name '*.po' | \
		while read -r file; do msgfmt "$$file" -o "$${file%po}mo"; done

install:
	mkdir -p "$(DPREFIX)/bin" "$(DPREFIX)/share/man/man1"
	find po -type d -maxdepth 2 -mindepth 2 | while read -r path;               \
	do                                                                          \
		mkdir -p "$(PODIR)/$${path#*/}";                                        \
		msgfmt "$$path/tiktok.po" -o "$(PODIR)/$${path#*/}/tiktok.mo";          \
	done
	cp tiktok "$(DPREFIX)/bin"

clean:
	rm tiktok
	find po -name '*.mo' -delete
