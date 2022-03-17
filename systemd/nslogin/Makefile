SRC=nslogin.c
LIBS=-lsystemd
CFLAGS=-Wall -Wextra -Wconversion -pedantic
CMODEFLAGS=-O0 -g -DDEBUG
POSTBUILD=chown root:root $@ && chmod ug+s $@
LINTER=clang-tidy-12


nslogin: $(SRC)
	$(CC) $(CFLAGS) $(CMODEFLAGS) $(SRC) $(LIBS) -o $@
	$(POSTBUILD)


.PHONY: lint install clean


lint:
	$(LINTER) $(SRC) -- $(CFLAGS) $(LIBS) 


Release: CMODEFLAGS=-O3 -DNDEBUG
Release: nslogin


install: Release
	mkdir -p $(DESTDIR)$(libexecdir)
	$(INSTALL) -o root -m 6755 ./nslogin $(DESTDIR)$(libexecdir)


clean:
	rm -f $(DESTDIR)$(libexecdir)/nslogin

