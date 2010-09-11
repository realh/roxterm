if BUILD_MANPAGES

esmandir = $(mandir)/es/man1
noinst_DATA = roxterm.1.es roxterm-config.1.es
DISTCLEANFILES = $(noinst_DATA) roxterm.1.es.xml roxterm-config.1.es.xml
EXTRA_DIST = roxterm.1.es.po roxterm-config.1.es.po \
	roxterm.1.es.xml.in roxterm-config.1.es.xml.in

roxterm.1.es: roxterm.1.es.xml
	@XMLTOMAN@ $<
	mv roxterm.1 roxterm.1.es

roxterm-config.1.es: roxterm-config.1.es.xml
	@XMLTOMAN@ $<
	mv roxterm-config.1 roxterm-config.1.es

roxterm.1.es.xml: roxterm.1.es.xml.in
	sed "s/@PACKAGE_VERSION@/$(PACKAGE_VERSION)/; s/@pkgdocdir@/$(pkgdocdir)/" \
		< $< > $@

roxterm-config.1.es.xml: roxterm-config.1.es.xml.in
	sed "s/@PACKAGE_VERSION@/$(PACKAGE_VERSION)/; s/@pkgdocdir@/$(pkgdocdir)/" \
		< $< > $@

roxterm.1.es.xml.in: roxterm.1.es.po
	po4a-translate -k 0 -f docbook -m ../roxterm.1.xml.in -p $< -l $@

roxterm-config.1.es.xml.in: roxterm-config.1.es.po
	po4a-translate -k 0 -f docbook -m ../roxterm-config.1.xml.in -p $< -l $@

roxterm.1.es.po: ../roxterm.1.xml.in
	po4a-updatepo -f docbook -m $< -p $@

roxterm-config.1.es.po: ../roxterm-config.1.xml.in
	po4a-updatepo -f docbook -m $< -p $@

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

# HTML translations

guide.html.pot: ../Help/en/guide.html
	po4a-gettextize -f xhtml -m $< -p $@

index.html.pot: ../Help/en/index.html
	po4a-gettextize -f xhtml -m $< -p $@

installation.html.pot: ../Help/en/installation.html
	po4a-gettextize -f xhtml -m $< -p $@

news.html.pot: ../Help/en/news.html
	po4a-gettextize -f xhtml -m $< -p $@

guide.html.es.po: ../Help/en/guide.html
	po4a-updatepo -f xhtml -m $< -p $@

index.html.es.po: ../Help/en/index.html
	po4a-updatepo -f xhtml -m $< -p $@

installation.html.es.po: ../Help/en/installation.html
	po4a-updatepo -f xhtml -m $< -p $@

news.html.es.po: ../Help/en/news.html
	po4a-updatepo -f xhtml -m $< -p $@

../Help/es/guide.html: guide.html.es.po
	po4a-translate -f xhtml -m ../Help/en/guide.html -p $< -l $@

../Help/es/index.html: index.html.es.po
	po4a-translate -f xhtml -m ../Help/en/index.html -p $< -l $@

../Help/es/installation.html: installation.html.es.po
	po4a-translate -f xhtml -m ../Help/en/installation.html -p $< -l $@

../Help/es/news.html: news.html.es.po
	po4a-translate -f xhtml -m ../Help/en/news.html -p $< -l $@

