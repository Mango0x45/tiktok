CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -std=c23 \
	-I$$(brew --prefix gettext)/include \
	-L$$(brew --prefix gettext)/lib \
	-lintl

all: tiktok

tiktok: main.c
	$(CC) $(CFLAGS) -o $@ $<

extract:
	xgettext --from-code=UTF-8 -k_ -o po/messages.pot main.c
	find po -name '*.po' -exec msgmerge {} po/messages.pot -o {} \;

translations:
	find po -name '*.po' | while read -r file; do msgfmt "$$file" -o "$${file%po}mo"; done

clean:
	rm tiktok
	find po -name '*.mo' -delete
