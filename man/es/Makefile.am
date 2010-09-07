if BUILD_MANPAGES

esmandir = $(mandir)/es/man1
esman_MANS = roxterm.1 roxterm-config.1
noinst_pkgdata_DATA = $(esman_MANS)
DISTCLEANFILES = $(esman_MANS)

roxterm.1: roxterm.1.xml
	@XMLTOMAN@ $<

roxterm-config.1: roxterm-config.1.xml
	@XMLTOMAN@ $<

install-data-local:
	$(install_sh) -m 0755 -d $(DESTDIR)/$(esmandir)
	$(install_sh) -m 0644 -t $(DESTDIR)/$(esmandir) $(esman_MANS)

uninstall-local:
	rm -f $(DESTDIR)/$(esmandir)/roxterm.1 \
		$(DESTDIR)/$(esmandir)/roxterm-config.1

endif

