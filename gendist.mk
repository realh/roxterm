DOC_DIR		= Help
IMG_DIR		= $(DOC_DIR)/lib
LOGO_SVG	= src/icons/roxterm.svg
LOGO_PNG	= $(IMG_DIR)/roxterm_logo.png
FAVICON		= $(IMG_DIR)/favicon.ico
TEXT_LOGO	= $(IMG_DIR)/logo_text.png
TEXT_ONLY	= $(IMG_DIR)/logo_text_only.png
ALL		= $(TEXT_LOGO) $(FAVICON) $(LOGO_PNG) \
		  $(ROXTERM_MAN) $(ROXTERM_CONFIG_MAN) 

all: $(ALL)

$(LOGO_PNG): $(LOGO_SVG)
	$(CONVERT_SVG) $@ $?

# Specify the inputs explicitly here in case $? doesn't guarantee they're
# in the right order
$(TEXT_LOGO): $(LOGO_PNG) $(TEXT_ONLY)
	$(COMPOSITE) -gravity SouthWest $(LOGO_PNG) $(TEXT_ONLY) $@

$(FAVICON): $(LOGO_PNG)
	$(CONVERT) $? -geometry 16x16 $@

DTD = http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl
XMLTOMAN_OPTS	= --novalid --param man.charmap.use.subset 0 $(DTD)
XMLTOMAN	= $(XSLTPROC) -o $@ $(XMLTOMAN_OPTS) $?

$(ROXTERM_MAN): roxterm.1.xml
	$(XSLTPROC) -o $@ $(XMLTOMAN_OPTS) $?

$(ROXTERM_CONFIG_MAN): roxterm-config.1.xml
	$(XSLTPROC) -o $@ $(XMLTOMAN_OPTS) $?

clean:
	rm -f $(ALL)
