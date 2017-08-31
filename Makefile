.PHONY: test

CFLAGS := -DDEBUG 

all: test

test:
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) jsmn.c test.c $(LDLIBS) -lcurl -g2

clean : rm test *.o
