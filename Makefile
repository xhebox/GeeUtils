.PHONY: test cli

CFLAGS := -DDEBUG -Wall -std=c11 -D_GNU_SOURCE
LDFLAGS := -lm

all: test cli

test:
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) jsmn.c test.c $(LDLIBS) -lcurl -g2

cli:
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) jsmn.c cli.c $(LDLIBS) -lcurl -g2

clean:
	rm -f test cli *.o
