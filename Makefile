# Makefile for respond
#
# Requirements:
#  - g++
#  - install
#  - gzip (only when using a compressed manpage)
#
# (c) 2007-2014 - Jouke Witteveen

BINDIR?=	$(PREFIX)/bin
CXX?=	g++
CXXFLAGS?=	-O2
INSTALL_MAN?=	install
INSTALL_PROGRAM?=	install
MANPAGE=	respond.1$(MANEXT)
MANPREFIX?=	$(PREFIX)
PTHREAD_LIBS?=	-pthread
PREFIX?=	/usr/local

.ALLSRC?=	$^
.TARGET?=	$@
OBJS=	main.o\
	process.o\
	respond.o


.PHONY:	all
all:	respond $(MANPAGE)

respond:	$(OBJS)
	$(CXX) $(CXXFLAGS) $(PTHREAD_LIBS) -o $(.TARGET) $(OBJS)

main.o:	main.cpp process.h respond.h
process.o:	process.cpp process.h
respond.o:	respond.cpp respond.h

respond.1.gz:	respond.1
	gzip -cn9 $(.ALLSRC) > $(.TARGET)

.PHONY:	install
install:	all
	$(INSTALL_PROGRAM) respond $(DESTDIR)$(BINDIR)/respond
	$(INSTALL_MAN) $(MANPAGE) $(DESTDIR)$(MANPREFIX)/man/man1/$(MANPAGE)

.PHONY:	uninstall
uninstall:
	-rm -f $(BINDIR)/respond $(MANPREFIX)/man/man1/$(MANPAGE)

.PHONY:	clean
clean:
	-rm -f $(OBJS) respond.1.gz respond
