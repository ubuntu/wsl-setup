SRC=nslogin.c
LIBS=-lsystemd
CFLAGS=-Wall -Wextra -Wconversion -pedantic
CMODEFLAGS=-O0 -g -DDEBUG
POSTBUILD=sudo chown root:root $@ && sudo chmod a+s $@
LINTER=clang-tidy-12

nslogin: $(SRC)
	$(CC) $(CFLAGS) $(CMODEFLAGS) $(SRC) $(LIBS) -o $@
	$(POSTBUILD)

.PHONY: lint

lint:
	$(LINTER) $(SRC) -- $(CFLAGS) $(LIBS) 

Release: CMODEFLAGS=-O3 -DNDEBUG
Release: nslogin
