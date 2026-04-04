#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#
# Modified by Wasted Audio
#

include ../../dep/dpf/Makefile.base.mk

all: plugin gen

# --------------------------------------------------------------

dgl:
# ifeq ($(HAVE_CAIRO_OR_OPENGL),true)
# 	$(MAKE) -C ../../dep/dpf/dgl FILE_BROWSER_DISABLED=true
# endif

plugin: dgl
	$(MAKE) all -C plugin/source

ifneq ($(CROSS_COMPILING),true)
gen: plugin lv2_ttl_generator
	@$(CURDIR)/../../dep/dpf/utils/generate-ttl.sh

lv2_ttl_generator:
	$(MAKE) -C ../../dep/dpf/utils/lv2-ttl-generator
else
gen:
endif

# --------------------------------------------------------------

clean:
	$(MAKE) clean -C ../../dep/dpf/utils/lv2-ttl-generator
	$(MAKE) clean -C plugin/source
	rm -rf bin build

# --------------------------------------------------------------

.PHONY: plugin