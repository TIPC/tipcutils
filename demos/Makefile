#
# Makefile for TIPC demos
#

SUBDIRS = hello_world connection_demo topology_subscr_demo multicast_demo \
	  stream_demo inventory_sim benchmark

.PHONY: subdirs $(SUBDIRS) clean

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

