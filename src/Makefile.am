AM_CPPFLAGS = @VMOD_INCLUDES@ -Wall -Werror

vmoddir = @VMOD_DIR@
vmod_LTLIBRARIES = libvmod_gossip.la

libvmod_gossip_la_LDFLAGS = -module -export-dynamic -avoid-version

libvmod_gossip_la_SOURCES = \
	vmod_gossip.c \
	vtree.h

nodist_libvmod_gossip_la_SOURCES = \
	vcc_if.c \
	vcc_if.h

vmod_gossip.lo: vcc_if.c vcc_if.h

vcc_if.c: vcc_if.h

vcc_if.h: @VMODTOOL@ $(top_srcdir)/src/vmod_gossip.vcc
	@VMODTOOL@ $(top_srcdir)/src/vmod_gossip.vcc

VMOD_TESTS = $(top_srcdir)/src/tests/*.vtc
.PHONY: $(VMOD_TESTS)

$(top_srcdir)/src/tests/*.vtc: libvmod_gossip.la
	@VARNISHTEST@ $@ -Dvarnishd=@VARNISHD@ -Dvmod_topbuild=$(abs_top_builddir)

check: $(VMOD_TESTS)

EXTRA_DIST = \
	vmod_gossip.vcc \
	$(VMOD_TESTS)

CLEANFILES = \
	$(builddir)/vcc_if.c \
	$(builddir)/vcc_if.h \
	$(builddir)/vmod_gossip.rst \
	$(builddir)/vmod_gossip.man.rst
