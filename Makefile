CC=gcc
PACKAGE=mqtt-sn-tools
VERSION=0.0.3
CFLAGS=-g -Wall -DVERSION=$(VERSION)
LDFLAGS=
TARGETS=mqtt-sn-pub mqtt-sn-sub


all: $(TARGETS)

mqtt-sn-pub: mqtt-sn.o mqtt-sn-pub.o
	$(CC) $(LDFLAGS) -o mqtt-sn-pub $^
  
mqtt-sn-pub.o: mqtt-sn-pub.c mqtt-sn.h
	$(CC) $(CFLAGS) -c mqtt-sn-pub.c

mqtt-sn-sub: mqtt-sn.o mqtt-sn-sub.o
	$(CC) $(LDFLAGS) -o mqtt-sn-sub $^
  
mqtt-sn-sub.o: mqtt-sn-sub.c mqtt-sn.h
	$(CC) $(CFLAGS) -c mqtt-sn-sub.c

mqtt-sn.o: mqtt-sn.c mqtt-sn.h
	$(CC) $(CFLAGS) -c mqtt-sn.c

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
