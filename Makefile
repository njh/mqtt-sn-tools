CC=gcc
PACKAGE=mqtts-client
VERSION=0.0.1
CFLAGS=-g -Wall -DVERSION=$(VERSION)
LDFLAGS=



all: mqtts-pub

mqtts-pub: mqtts-pub.o
	$(CC) $(LDFLAGS) -o mqtts-pub mqtts-pub.o
  
clean:
	rm -f *.o mqtts-pub
	
dist:
	distdir='$(PACKAGE)-$(VERSION)'; mkdir $$distdir || exit 1; \
	list=`git ls-files`; for file in $$list; do \
		cp -pR $$file $$distdir || exit 1; \
	done; \
	tar -zcf $$distdir.tar.gz $$distdir; \
	rm -fr $$distdir

.PHONY: all clean dist
