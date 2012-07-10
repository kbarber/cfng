man_MANS = cfengine.8 cfagent.8  cfenvd.8  cfenvgraph.8  cfexecd.8  cfkey.8  cfrun.8  cfservd.8

docdir = $(prefix)/doc/$(PACKAGE)-$(VERSION)
doc_DATA =
# Info used in building and installing HTML files

# Make sure these get distributed with everything else.
EXTRA_DIST = cfagent.8 cfenvd.8 cfenvgraph.8 cfexecd.8 cfkey.8 cfrun.8 cfservd.8 cfengine.8 

MOSTLYCLEANFILES = *~ *.orig

#
# Get everything removed down to where rebuilding requires:
# "make; make install"
# 
CLEANFILES = $(htmlfiles) $(psfiles)

#
# Get everything removed down to where rebuilding requires:
# "configure; make; make install"
#
DISTCLEANFILES = *.log *.aux *.mbs *.mb *.info*

#
# Get everything removed down to where rebuilding requires:
# "aclocal; autoconf; autoheader; automake --add-missing"
# "configure; make; make install"
#
MAINTAINERCLEANFILES = Makefile.in mdate-sh
