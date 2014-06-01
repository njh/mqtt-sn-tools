CC=cc
PACKAGE=mqtt-sn-tools
VERSION=0.0.3
CFLAGS=-g -Wall -DVERSION=$(VERSION)
LDFLAGS=
TARGETS=mqtt-sn-pub mqtt-sn-sub mqtt-sn-serial-bridge


all: $(TARGETS)

$(TARGETS): %: mqtt-sn.o %.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o : %.c mqtt-sn.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(TARGETS)

dist:
	distdir='$(PACKAGE)-$(VERSION)'; mkdir $$distdir || exit 1; \
	list=`git ls-files`; for file in $$list; do \
		cp -pR $$file $$distdir || exit 1; \
	done; \
	tar -zcf $$distdir.tar.gz $$distdir; \
	rm -fr $$distdir

.PHONY: all clean dist
