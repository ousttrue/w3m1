#
# w3mimg/Makefile
#

SHELL=/bin/sh
srcdir = .
top_srcdir = ..
VPATH = .
CFLAGS=$(OPTS) -I.. -I$(top_srcdir) -I$(srcdir) -g -O2 -I$(srcdir)/libwc  -I/usr/include/openssl  -DHAVE_CONFIG_H $(IMGCFLAGS)
AR=ar
RANLIB=ranlib
RM=rm

SUBDIRS=fb x11 win
IMGCFLAGS=  
IMGOBJS=w3mimg.o

.PHONY: $(SUBDIRS)
all:  w3mimg.a

w3mimg.a: $(IMGOBJS)
	$(AR) rv $@ $(IMGOBJS)
	$(RANLIB) $@

w3mimg.o: w3mimg.c
	$(CC) $(CFLAGS) -c $<

$(SUBDIRS):
	cd $@ && $(MAKE) CC="$(CC)" OPTS="$(OPTS)"

clean:
	@-$(RM) -f *.o
	@for dir in $(SUBDIRS); do \
		(cd $$dir && $(MAKE) clean RM=$(RM)); \
	 done
	-$(RM) -f w3mimg.a

distclean: clean
	for subdir in $(SUBDIRS); \
	do \
		(cd $$subdir && $(MAKE) distclean); \
	done
	-$(RM) -f Makefile


#
