#!/bin/sh

d="`dirname $0`"
PAGES='guide index installation news'
LANGS=`cat "$d/LINGUAS"`

f="$d/Makefile.am"

echo >$f '# Generated by genmake.sh'
echo >>$f
echo >>$f 'Makefile.am: genmake.sh'
printf >>$f '\t./genmake.sh\n'
echo >>$f
echo >>$f 'PO4ACHARSET = -M UTF-8'
printf >>$f 'PO4AOPTS = $(PO4ACHARSET) \\\n'
printf >>$f '\t\t--package-name=$(PACKAGE) --package-version=$(VERSION) \\\n'
printf >>$f '\t\t--copyright-holder="Tony Houghton" \\\n'
printf >>$f '\t\t--msgid-bugs-address=h@realh.co.uk \n\n'
echo >>$f 'PO4A_GETTEXTIZE = po4a-gettextize $(PO4AOPTS) -L UTF-8'
echo >>$f 'PO4A_UPDATEPO = po4a-updatepo $(PO4AOPTS)'
echo >>$f 'PO4A_TRANSLATE = po4a-translate $(PO4ACHARSET) -L UTF-8'
echo >>$f
echo >>$f 'if BUILD_MANPAGES'
echo >>$f

TRANSLATED_MANS=''
MAN_DISTCLEANFILES=''
MAN_POFILES=''
MAN_EXTRA_DIST=''
for l in $LANGS
do
    echo >>$f ${l}mandir = '$(mandir)'/$l/man1
    for r in roxterm roxterm-config
    do
        TRANSLATED_MANS="$TRANSLATED_MANS $l/$r.1"
        MAN_DISTCLEANFILES="$MAN_DISTCLEANFILES $r.1.$l.xml"
        MAN_POFILES="$MAN_POFILES $r.1.$l.po"
        MAN_EXTRA_DIST="$MAN_EXTRA_DIST $r.1.$l.xml.in"
    done
done

echo >>$f
echo >>$f TRANSLATED_MANS = $TRANSLATED_MANS
echo >>$f MAN_DISTCLEANFILES = '$(TRANSLATED_MANS)' $DISTCLEANFILES
echo >>$f MAN_POFILES = $MAN_POFILES
echo >>$f MAN_EXTRA_DIST = '$(MAN_POFILES)' $MAN_EXTRA_DIST
echo >>$f
echo >>$f if ENABLE_PO4A
echo >>$f

POTFILES=''
for r in roxterm roxterm-config
do
    POTFILES="$POTFILES $r.1.pot"
    echo >>$f $r.1.pot: ../$r.1.xml.in
    printf >>$f '\t$(PO4A_GETTEXTIZE) -f docbook -m $< -p $@\n'
    printf >>$f '\tsed -i s/charset=CHARSET/charset=UTF-8/ $@\n\n'
done

for l in $LANGS
do
    for r in roxterm roxterm-config
    do
        echo >>$f endif
        echo >>$f
        echo >>$f $l/$r.1: $r.1.$l.xml
        printf >>$f '\t$(MKDIR_P) %s\n' $l
        printf >>$f '\t@XMLTOMAN@ -o %s/ @XMLTOMAN_OPTS@ $<\n\n' $l
        echo >>$f $r.1.$l.xml: $r.1.$l.xml.in
        printf >>$f '\tsed "s/\\@PACKAGE_VERSION\\@/$(PACKAGE_VERSION)/; s#\\@htmldir\\@#$(htmldir)#" < $< > $@\n\n'
        echo >>$f if ENABLE_PO4A
        echo >>$f
        echo >>$f $r.1.$l.xml.in: $r.1.$l.po
        printf >>$f '\t$(PO4A_TRANSLATE) -k 0 -f docbook -m ../%s.1.xml.in -p $< -l $@\n\n' $r
        echo >>$f $r.1.$l.po: ../$r.1.xml.in $r.1.pot
        printf >>$f '\t$(PO4A_UPDATEPO) -f docbook -m $< -p $@\n'
        printf >>$f '\tsed -i s/charset=CHARSET/charset=UTF-8/ $@\n\n'
    done
done

echo >>$f endif
echo >>$f

echo >>$f install-mans:
for l in $LANGS
do
	printf >>$f '\t$(install_sh) -m 0755 -d $(DESTDIR)/$(%smandir)\n' $l
    for r in roxterm roxterm-config
    do
        printf >>$f '\t$(install_sh) %s/%s.1 $(DESTDIR)/$(%smandir)/\n' $l $r $l
    done
done

echo >>$f
echo >>$f uninstall-local:
for l in $LANGS
do
    for r in roxterm roxterm-config
    do
        printf >>$f '\trm -f $(DESTDIR)/$(%smandir)/%s.1\n' $l $r
    done
done
echo >>$f
echo >>$f else
echo >>$f
echo >>$f install-mans:
echo >>$f
echo >>$f endif
echo >>$f

echo >>$f '# HTML translations'
echo >>$f

printf >>$f 'HTML_POFILES ='
for l in $LANGS
do
    for p in $PAGES
    do
        printf >>$f ' \\\n\t\t%s.html.%s.po' $p $l
    done
done
echo >>$f
echo >>$f
for l in $LANGS
do
    printf >>$f 'HTMLFILES_%s =' `echo $l | tr '[:lower:]' '[:upper:]'`
    for p in $PAGES
    do
        printf >>$f ' \\\n\t\t../Help/%s/%s.html' $l $p
    done
    echo >>$f
    echo >>$f
done
echo >>$f
echo >>$f install-data-local: install-mans
for l in $LANGS
do
    printf >>$f '\t$(install_sh) -m 0755 -d $(DESTDIR)/$(htmldir)/%s\n' $l
	printf >>$f '\t$(install_sh) -m 0644 -t $(DESTDIR)/$(htmldir)/%s $(HTMLFILES_%s)\n' $l `echo $l | tr '[:lower:]' '[:upper:]'`
done
echo >>$f
echo >>$f if ENABLE_PO4A
echo >>$f

for p in $PAGES
do
    POTFILES="$POTFILES $p.html.pot"
    echo >>$f $p.html.pot: ../Help/en/$p.html
    printf >>$f '\t$(PO4A_GETTEXTIZE) -f xhtml -m $< -p $@\n'
    printf >>$f '\tsed -i "s/charset=CHARSET/charset=UTF-8/" $@\n'
    printf >>$f '\tsed -i "s/SOME DESCRIPTIVE TITLE/Translations for roxterm docs/" $@\n'
    printf >>$f '\tsed -i "s/Copyright (C) YEAR/Copyright (C) 2010/" $@\n'
    printf >>$f '\tsed -i "s/FIRST AUTHOR <EMAIL@ADDRESS>, YEAR/Tony Houghton <h@realh.co.uk>, 2010/" $@\n'
    echo >>$f
    for l in $LANGS
    do
        echo >>$f $p.html.$l.po: ../Help/en/$p.html $p.html.pot
        printf >>$f '\t$(PO4A_UPDATEPO) -f xhtml -m $< -p $@\n'
        echo >>$f
        echo >>$f ../Help/$l/$p.html: $p.html.$l.po
        printf >>$f '\t$(PO4A_TRANSLATE) -k 0 -f xhtml -m ../Help/en/%s.html -p $< -l $@\n\n' $p
    done
done

echo >>$f endif
echo >>$f
echo >>$f '# Force building and distribution of various files'
echo >>$f POTFILES = "$POTFILES"
TRANSLATED_HTML=''
for l in $LANGS
do
    TRANSLATED_HTML="$TRANSLATED_HTML "'$(HTMLFILES_'`echo $l | tr '[:lower:]' '[:upper:]'`')'
done
echo >>$f TRANSLATED_HTML = "$TRANSLATED_HTML"
echo >>$f if ENABLE_PO4A
echo >>$f 'EXTRA_DIST = $(MAN_EXTRA_DIST) $(HTML_POFILES) $(TRANSLATED_HTML) $(POTFILES)'
echo >>$f 'noinst_DATA = $(TRANSLATED_MANS) $(TRANSLATED_HTML) $(POTFILES)'
echo >>$f else
echo >>$f 'EXTRA_DIST = $(MAN_EXTRA_DIST) $(HTML_POFILES) $(TRANSLATED_HTML)'
echo >>$f 'noinst_DATA = $(TRANSLATED_MANS) $(TRANSLATED_HTML)'
echo >>$f endif
echo >>$f 'DISTCLEANFILES = $(MAN_DISTCLEANFILES)'

