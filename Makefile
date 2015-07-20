CC=cc
PACKAGE=mqtt-sn-tools
VERSION=0.0.3
CFLAGS=-g -Wall -DVERSION=$(VERSION)
LDFLAGS=
INSTALL?=install
prefix=/usr/local

TARGETS=mqtt-sn-pub mqtt-sn-sub mqtt-sn-serial-bridge

.PHONY : all install uninstall clean dist test


all: $(TARGETS)

$(TARGETS): %: mqtt-sn.o %.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o : %.c mqtt-sn.h
	$(CC) $(CFLAGS) -c $<

install: $(TARGETS)
	$(INSTALL) -d "$(DESTDIR)$(prefix)/bin"
	$(INSTALL) -s $(TARGETS) "$(DESTDIR)$(prefix)/bin"

uninstall:
	@for target in $(TARGETS); do \
		cmd="rm -f $(DESTDIR)$(prefix)/bin/$$target"; \
		echo "$$cmd" && $$cmd; \
	done

clean:
	-rm -f *.o $(TARGETS)

dist:
	distdir='$(PACKAGE)-$(VERSION)'; mkdir $$distdir || exit 1; \
	list=`git ls-files`; for file in $$list; do \
		cp -pR $$file $$distdir || exit 1; \
	done; \
	tar -zcf $$distdir.tar.gz $$distdir; \
	rm -fr $$distdir

test: all
	cd test && rake test

.PHONY: all clean dist
