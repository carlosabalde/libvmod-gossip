AM_CFLAGS = $(VARNISHAPI_CFLAGS) $(CODE_COVERAGE_CFLAGS) -Wall -Werror
AM_LDFLAGS = $(VARNISHAPI_LIBS) $(VMOD_LDFLAGS) $(CODE_COVERAGE_LDFLAGS)

vmod_LTLIBRARIES = libvmod_gossip.la

libvmod_gossip_la_SOURCES = \
	vmod_gossip.c \
	vtree.h

nodist_libvmod_gossip_la_SOURCES = \
	vcc_gossip_if.c \
	vcc_gossip_if.h

dist_man_MANS = vmod_gossip.3

@BUILD_VMOD_GOSSIP@

AM_TESTS_ENVIRONMENT = \
	PATH="$(VMOD_TEST_PATH)" \
	LD_LIBRARY_PATH="$(VARNISH_LIBRARY_PATH)"
TEST_EXTENSIONS = .vtc
VTC_LOG_COMPILER = varnishtest
AM_VTC_LOG_FLAGS = -v -Dvmod_gossip="$(VMOD_GOSSIP)"

TESTS = @VMOD_TESTS@

EXTRA_DIST = \
	vmod_gossip.vcc \
	$(VMOD_TESTS)
