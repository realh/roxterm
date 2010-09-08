if BUILD_MANPAGES

esmandir = $(mandir)/es/man1
noinst_DATA = roxterm.1.es roxterm-config.1.es
DISTCLEANFILES = $(noinst_DATA)

roxterm.1.es: roxterm.1.es.xml
	@XMLTOMAN@ $<
	mv roxterm.1 roxterm.1.es

roxterm-config.1.es: roxterm-config.1.es.xml
	@XMLTOMAN@ $<
	mv roxterm-config.1 roxterm-config.1.es

install-data-local:
	$(install_sh) -m 0755 -d $(DESTDIR)/$(esmandir)
	cp roxterm.1.es $(DESTDIR)/$(esmandir)/roxterm.1
	chmod 0644 $(DESTDIR)/$(esmandir)/roxterm.1
	cp roxterm-config.1.es $(DESTDIR)/$(esmandir)/roxterm-config.1
	chmod 0644 $(DESTDIR)/$(esmandir)/roxterm-config.1

uninstall-local:
	rm -f $(DESTDIR)/$(esmandir)/roxterm.1 \
		$(DESTDIR)/$(esmandir)/roxterm-config.1

endif

