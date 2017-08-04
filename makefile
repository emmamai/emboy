CC ?= gcc
CFLAGS ?= -g

emboy: emboy.c
    $(CC) $(CFLAGS) -o $@ $<

clean:
	-@rm emboy