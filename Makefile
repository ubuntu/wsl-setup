# GNU Standard says we should not assign DESTDIR, but this minor adjustment is really handy when debugging.
DESTDIR:=$(if $(DESTDIR), $(shell realpath $(DESTDIR)),)
export INSTALL=install

# Standard GNU Makefile definitions:
export prefix = /usr
export exec_prefix = $(prefix)
export libexecdir = $(exec_prefix)/libexec


.PHONY: install clean

nslogin:
	$(MAKE) -C ./systemd/nslogin/


install:
	mkdir -p $(DESTDIR)$(libexecdir)
	$(MAKE) -C ./systemd/nslogin/ install
	$(INSTALL) -o root -m 755 ./wsl-setup $(DESTDIR)$(libexecdir)
	$(INSTALL) -o root -m 755 ./systemd/wsl-systemd $(DESTDIR)$(libexecdir)


clean:
	$(MAKE) -C ./systemd/nslogin/ clean
	rm -f $(DESTDIR)$(libexecdir)/wsl-setup
	rm -f $(DESTDIR)$(libexecdir)/wsl-systemd

