# Copyright (C) International Business Machines Corp., 2005
# Author: Josh Triplett <josh@kernel.org>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; under version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

XEN_ROOT=../../..
include $(XEN_ROOT)/tools/Rules.mk

ifneq ($(XENSTAT_XENTOP),y)
.PHONY: all install xenstat
all install xenstat:
else

CFLAGS += -DGCC_PRINTF -Wall -I$(XEN_LIBXENSTAT)
#CFLAGS += -DGCC_PRINTF -Wall -Werror -I$(XEN_LIBXENSTAT)
LDFLAGS += -L$(XEN_LIBXENSTAT)
LDFLAGS += $(call LDFLAGS_RPATH,../lib)
LDLIBS += -lxenstat $(CURSES_LIBS) $(SOCKET_LIBS)
CFLAGS += -DHOST_$(XEN_OS)

.PHONY: all
all: xenstat

.PHONY: install
install: xenstat xenstat.1
	$(INSTALL_DIR) $(DESTDIR)$(PRIVATE_BINDIR)
	$(INSTALL_PROG) xenstat $(DESTDIR)$(PRIVATE_BINDIR)/xenstat
	$(INSTALL_DIR) $(DESTDIR)$(MAN1DIR)
	$(INSTALL_DATA) xenstat.1 $(DESTDIR)$(MAN1DIR)/xenstat.1

endif

.PHONY: clean
clean:
	rm -f xenstat xenstat.o $(DEPS)

-include $(DEPS)
