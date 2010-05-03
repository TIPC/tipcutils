#
# Makefile for TIPC utilities package
#

export VERSION = 2.0.0

SUBDIRS = tipc-config ptts demos

.PHONY: subdirs $(SUBDIRS) clean

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

dist: clean
	git tar-tree HEAD tipcutils-$(VERSION) | gzip - > ../tipcutils-$(VERSION).tar.gz

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

