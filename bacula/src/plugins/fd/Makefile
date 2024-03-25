#
# Simple Makefile for building test FD plugins for Bacula
#
# Copyright (C) 2000-2023 Kern Sibbald
# License: BSD 2-Clause; see file LICENSE-FOSS
#
#

include Makefile.inc


# No optimization for now for easy debugging

KUBESRC = kubernetes-fd.c kubernetes-fd.h
TESTKUBESRC = test_kubernetes_backend.c
KUBEOBJ = $(KUBESRC:.c=.lo) $(METAPLUGINOBJ)
TESTKUBEOBJ = $(TESTKUBESRC:.c=.lo)

.SUFFIXES:    .c .lo

.c.lo:
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) -I${SRCDIR} -I${FDDIR} -I${LIBDIR} -I. -c $<

all: bpipe-fd.la

test-plugins: test-plugin-fd.la test-deltaseq-fd.la test-handlexacl-plugin-fd.la

test_kubernetes_backend.lo: $(TESTKUBESRC)
	@echo "Compiling $< ..."
	$(NO_ECHO)$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I${SRCDIR} -I${FDDIR} -DLOGDIR=\"$(DESTDIR)$(working_dir)\" -c $<

kubernetes-fd.la: Makefile $(KUBEOBJ)
	@echo "Linking $(@:.la=.so) ..."
	$(NO_ECHO)$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) -shared $(KUBEOBJ) -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

install-kubernetes: kubernetes-fd.la
	$(MAKE) -C kubernetes-backend install-kubernetes
	$(MKDIR) $(DESTDIR)$(plugindir)
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) kubernetes-fd.la $(DESTDIR)$(plugindir)
	$(NO_ECHO)$(RMF) $(DESTDIR)$(plugindir)/kubernetes-fd.la

install-test-kubernetes: test_kubernetes_backend kubernetes-fd.la
	$(MKDIR) $(DESTDIR)$(plugindir)
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) kubernetes-fd.la $(DESTDIR)$(plugindir)
	$(NO_ECHO)$(RMF) $(DESTDIR)$(plugindir)/kubernetes-fd.la
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) test_kubernetes_backend $(DESTDIR)$(sbindir)/k8s_backend

cdp: cdp-fd.la

cdp-fd.lo: cdp-fd.c journal.h folder-record.h file-record.h settings-record.h ${FDDIR}/fd_plugins.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -Wno-unused-function -I../.. -I${FDDIR} -c cdp-fd.c

journal.lo: journal.c journal.h folder-record.h file-record.h settings-record.h ${FDDIR}/fd_plugins.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -Wno-unused-function -I../.. -I${FDDIR} -c journal.c

cdp-fd.la: Makefile cdp-fd$(DEFAULT_OBJECT_TYPE) journal.lo
	$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) -shared cdp-fd.lo journal.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

install-cdp: cdp-fd.la
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) cdp-fd.la $(DESTDIR)$(plugindir)
	$(RMF) $(DESTDIR)$(plugindir)/cdp-fd.la

rot13: rot13-fd.la

all: bpipe-fd.la test-plugin-fd.la test-deltaseq-fd.la

example-plugin-fd.lo: example-plugin-fd.c ${FDDIR}/fd_plugins.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I../.. -I${FDDIR} -c example-plugin-fd.c

example-plugin-fd.la: Makefile example-plugin-fd$(DEFAULT_OBJECT_TYPE)
	$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) -shared example-plugin-fd.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

bpipe-fd.lo: bpipe-fd.c ${FDDIR}/fd_plugins.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I../.. -I${FDDIR} -c bpipe-fd.c

bpipe-fd.la: Makefile bpipe-fd$(DEFAULT_OBJECT_TYPE)
	$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) -shared bpipe-fd.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

test-bpipe-fd.c: bpipe-fd.c
	cp -f bpipe-fd.c test-bpipe-fd.c

test-bpipe-fd.lo: test-bpipe-fd.c ${FDDIR}/fd_plugins.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -DTEST_BPIPE_OFFSET -I../.. -I${FDDIR} -c test-bpipe-fd.c

test-bpipe-fd.la: Makefile test-bpipe-fd$(DEFAULT_OBJECT_TYPE)
	$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) -shared test-bpipe-fd.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version 

rot13-fd.lo: rot13-fd.c ${FDDIR}/fd_plugins.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I../.. -I${FDDIR} -c rot13-fd.c

rot13-fd.la: Makefile rot13-fd$(DEFAULT_OBJECT_TYPE)
	$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) -shared rot13-fd.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version 

namedpipe.lo: namedpipe.c namedpipe.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -c namedpipe.c

test-deltaseq-fd.lo: test-deltaseq-fd.c ${FDDIR}/fd_plugins.h fd_common.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I../.. -I${FDDIR} -c test-deltaseq-fd.c

test-deltaseq-fd.la: Makefile test-deltaseq-fd$(DEFAULT_OBJECT_TYPE)
	$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) -shared test-deltaseq-fd.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

delta-fd.lo: delta-fd.c ${FDDIR}/fd_plugins.h fd_common.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(RSYNC_INC) $(CFLAGS) -I../.. -I${FDDIR} -c delta-fd.c

delta-fd.la: Makefile delta-fd$(DEFAULT_OBJECT_TYPE)
	$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) $(RSYNC_LIBS) -shared delta-fd.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version 

test-verify-fd.lo: test-verify-fd.c ${FDDIR}/fd_plugins.h fd_common.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(RSYNC_INC) $(CFLAGS) -I../.. -I${FDDIR} -c test-verify-fd.c

test-verify-fd.la: Makefile test-verify-fd$(DEFAULT_OBJECT_TYPE)
	$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) -shared test-verify-fd.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

install-antivirus: antivirus-fd.la
	$(MKDIR) $(DESTDIR)$(plugindir)
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) antivirus-fd.la $(DESTDIR)$(plugindir)
	$(RMF) $(DESTDIR)$(plugindir)/antivirus-fd.la

antivirus-fd.lo: antivirus-fd.c ${FDDIR}/fd_plugins.h fd_common.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I../.. -I${FDDIR} -c antivirus-fd.c

antivirus-fd.la: Makefile antivirus-fd$(DEFAULT_OBJECT_TYPE)
	$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) -shared antivirus-fd.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

test-plugin-fd.lo: test-plugin-fd.c ${FDDIR}/fd_plugins.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I../.. -I${FDDIR} -c test-plugin-fd.c

test-plugin-fd.la: Makefile test-plugin-fd$(DEFAULT_OBJECT_TYPE)
	$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) -shared test-plugin-fd.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

test-handlexacl-plugin-fd.lo: test-handlexacl-plugin-fd.c ${FDDIR}/fd_plugins.h
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I../.. -I${FDDIR} -c test-handlexacl-plugin-fd.c

test-handlexacl-plugin-fd.la: Makefile test-handlexacl-plugin-fd$(DEFAULT_OBJECT_TYPE)
	$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) -shared test-handlexacl-plugin-fd.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version 

test_kubernetes_backend: Makefile $(TESTKUBEOBJ) $(PLUGINLIBDIR)/test_metaplugin_backend.lo
	@echo "Building backend $@ ..."
	$(NO_ECHO)$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) test_kubernetes_backend.lo $(PLUGINLIBDIR)/test_metaplugin_backend.lo -o $@

install: all $(EXTRA_INSTALL_TARGET)
	$(MKDIR) $(DESTDIR)$(plugindir)
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) bpipe-fd.la $(DESTDIR)$(plugindir)
	$(RMF) $(DESTDIR)$(plugindir)/bpipe-fd.la

install-docker:
	$(MAKE) -C docker install-docker

install-test-plugin: all test-plugins test-bpipe-fd.la rot13-fd.la
	$(MKDIR) $(DESTDIR)$(plugindir)
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) test-plugin-fd.la $(DESTDIR)$(plugindir)
	$(RMF) $(DESTDIR)$(plugindir)/test-plugin-fd.la
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) test-deltaseq-fd.la $(DESTDIR)$(plugindir)
	$(RMF) $(DESTDIR)$(plugindir)/test-deltaseq-fd.la
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) test-handlexacl-plugin-fd.la $(DESTDIR)$(plugindir)
	$(RMF) $(DESTDIR)$(plugindir)/test-handlexacl-plugin-fd.la
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) test-bpipe-fd.la $(DESTDIR)$(plugindir)
	$(RMF) $(DESTDIR)$(plugindir)/test-bpipe-fd.la
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) rot13-fd.la $(DESTDIR)$(plugindir)
	$(RMF) $(DESTDIR)$(plugindir)/rot13-fd.la

install-test-dedup: test-dedup-fd.la
	$(MKDIR) $(DESTDIR)$(plugindir)
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) test-dedup-fd.la $(DESTDIR)$(plugindir)
	$(RMF) $(DESTDIR)$(plugindir)/test-dedup-fd.la

install-delta-plugin: all delta-fd.la
	$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) delta-fd.la $(DESTDIR)$(plugindir)
	$(RMF) $(DESTDIR)$(plugindir)/delta-fd.la

libtool-clean:
	@find . -name '*.lo' -print | xargs $(LIBTOOL_CLEAN) $(RMF)
	@$(RMF) *.la
	@$(RMF) -r .libs _libs

clean:	libtool-clean
	@rm -f main *.so *.o 1 2 3 *_test ndmp_idx_dump

distclean: clean
	@rm -f Makefile

libtool-uninstall:
	$(LIBTOOL_UNINSTALL) $(RMF) $(DESTDIR)$(plugindir)/bpipe-fd.so

uninstall: $(LIBTOOL_UNINSTALL_TARGET)

depend:
