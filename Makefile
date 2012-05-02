#
# Makefile for TIPC utilities package
#

export VERSION = 2.0.2

SUBDIRS = tipc-config tipc-pipe ptts demos

.PHONY: subdirs $(SUBDIRS) clean

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

dist: clean
	git archive --format=tar --prefix=tipcutils-$(VERSION)/ HEAD | gzip - > ../tipcutils-$(VERSION).tar.gz

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

