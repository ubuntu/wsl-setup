SRC=nslogin.c
LIBS=-lsystemd
CFLAGS=-Wall -Wextra -pedantic
POSTBUILD=sudo chown root:root $@ && sudo chmod a+s $@
LINTER=clang-tidy-12

nslogin: $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LIBS) -o $@
	$(POSTBUILD)

.PHONY: lint

lint:
	$(LINTER) $(SRC) -- $(CFLAGS) $(LIBS) 

