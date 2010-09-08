if BUILD_MANPAGES

esmandir = $(mandir)/es/man1
noinst_DATA = roxterm.es.1 roxterm-config.es.1
DISTCLEANFILES = $(noinst_DATA)

roxterm.es.1: roxterm.es.1.xml
	@XMLTOMAN@ $<
	mv roxterm.1 roxterm.es.1

roxterm-config.es.1: roxterm-config.es.1.xml
	@XMLTOMAN@ $<
	mv roxterm-config.1 roxterm-config.es.1

install-data-local:
	$(install_sh) -m 0755 -d $(DESTDIR)/$(esmandir)
	cp roxterm.es.1 $(DESTDIR)/$(esmandir)/roxterm.1
	chmod 0644 $(DESTDIR)/$(esmandir)/roxterm.1
	cp roxterm-config.es.1 $(DESTDIR)/$(esmandir)/roxterm-config.1
	chmod 0644 $(DESTDIR)/$(esmandir)/roxterm-config.1

uninstall-local:
	rm -f $(DESTDIR)/$(esmandir)/roxterm.1 \
		$(DESTDIR)/$(esmandir)/roxterm-config.1

endif

