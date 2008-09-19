#
# Makefile for TIPC utilities package
#

export VERSION = 1.1.8

SUBDIRS = tipc-config

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

