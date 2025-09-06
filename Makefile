CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -std=c23 \
	-I$$(brew --prefix gettext)/include \
	-L$$(brew --prefix gettext)/lib \
	-lintl
PODIR = /usr/local/share/locale

all: tiktok

tiktok: main.c
	$(CC) $(CFLAGS) -DPODIR='"$(PODIR)"' -o $@ $<

extract:
	xgettext --from-code=UTF-8 -k_ -o po/tiktok.pot main.c
	find po -name '*.po' -exec msgmerge {} po/tiktok.pot -o {} \;

translations:
	find po -name '*.po' | while read -r file; do msgfmt "$$file" -o "$${file%po}mo"; done

clean:
	rm tiktok
	find po -name '*.mo' -delete
