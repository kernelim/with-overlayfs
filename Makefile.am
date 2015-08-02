ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

SUBDIRS = .

bin_PROGRAMS = with-overlayfs
with_overlayfs_SOURCES = src/main.cpp

man_MANS = doc/with-overlayfs.1

doc/with-overlayfs.1: doc/with-overlayfs.1.md
	pandoc -s -t man $< -o $@

lib_LTLIBRARIES=libwith_overlayfs_after_load.la
libwith_overlayfs_after_load_la_SOURCES=src/after_load.c

install-exec-hook:
	mkdir -p $(DESTDIR)$(pkgdatadir)
	ln -f -s $(libdir)/libwith_overlayfs_after_load.so $(DESTDIR)$(pkgdatadir)/hook.so

$(eval include $(top_srcdir)/Makefile.src)
