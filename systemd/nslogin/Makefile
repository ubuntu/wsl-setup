SRC=nslogin.c
LIBS=-lsystemd
CFLAGS=-Wall -Wextra -Wconversion -pedantic -g
POSTBUILD=chown root:root $< && chmod ug+s $<
LINTER=clang-tidy-12


Release: CMODEFLAGS=-O2 -DNDEBUG
Release: nslogin


Debug: CMODEFLAGS=-O0 -DDEBUG
Debug: nslogin
	$(POSTBUILD)


nslogin: $(SRC)
	$(CC) $(CFLAGS) $(CMODEFLAGS) $(SRC) $(LIBS) -o $@


.PHONY: lint install clean


lint:
	$(LINTER) $(SRC) -- $(CFLAGS) $(LIBS)


install: Release
	mkdir -p $(DESTDIR)$(libexecdir)
	$(INSTALL) -o root -m 6755 ./nslogin $(DESTDIR)$(libexecdir)


clean:
	rm -f ./nslogin
	rm -f $(DESTDIR)$(libexecdir)/nslogin

