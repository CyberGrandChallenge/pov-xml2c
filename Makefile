BINARY   = pov-xml2c
MAN      = $(BINARY).1.gz
BIN      = $(DESTDIR)/usr/bin
MANDIR   = $(DESTDIR)/usr/share/man/man1
EXAMPLES = $(DESTDIR)/usr/share/pov2c/examples
OBJS     = xml2c_delay.o xml2c_read.o xml2c_write.o logging.o xml2c.o xml2c_negotiate.o xml2c_var.o utils.o

CC = g++
LD = g++

INC += -I/usr/include/libxml2
LIBS = -lpcre -lxml2

CFLAGS += -O3 -g -D_FORTIFY_SOURCE=2 -fstack-protector -fPIE
CFLAGS += -Werror -Wno-variadic-macros 
CFLAGS += -DRANDOM_UID -DHAVE_SETRESGID
CFLAGS += -Wno-delete-non-virtual-dtor
# CFLAGS += -fsanitize=undefined-trap -fsanitize-undefined-trap-on-error

LDFLAGS += -Wl,-z,relro -Wl,-z,now

all: $(BINARY) man

$(BINARY): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.cc
	$(CC) -c $(CFLAGS) $(INC) $< -o $@

%.1.gz: %.md
	pandoc -s -t man $< -o $<.tmp
	gzip -9 < $<.tmp > $@

man: $(MAN)

install: $(BINARY) $(MAN)
	ls -la $(MAN)
	install -d $(BIN)
	install $(BINARY) $(BIN)
	install -d $(MANDIR)
	install $(MAN) $(MANDIR)
	install -d $(EXAMPLES)
	install -m 444 examples/*.xml $(EXAMPLES)

clean:
	-@rm -f *.o $(BINARY) $(MAN) *.tmp

distclean: clean
