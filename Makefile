CFLAGS=-Wall
LDLIBS=-ltoxcore
OBJDIR=build
CC=gcc

all: CC += -march=native -O3
all:$(OBJDIR) $(OBJDIR)/main

debug: CC += -g
debug: $(OBJDIR) $(OBJDIR)/main

clean:
	$(RM) -f $(OBJDIR)/main $(OBJDIR)/main.o $(OBJDIR)/fileop.o $(OBJDIR)/filesend.o $(OBJDIR)/crc32.o $(OBJDIR)/md5.o $(OBJDIR)/sha256.o $(OBJDIR)/conf.o $(OBJDIR)/misc.o $(OBJDIR)/ylog.o

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/crc32.o: src/crypto/crc32.c
	$(CC) $(CFLAGS) $< -c -o $@

$(OBJDIR)/md5.o: src/crypto/md5.c
	$(CC) $(CFLAGS) $< -c -o $@

$(OBJDIR)/sha256.o: src/crypto/sha256.c
	$(CC) $(CFLAGS) $< -c -o $@

$(OBJDIR)/ylog.o: src/ylog.c
	$(CC) $(CFLAGS) $< -c -o $@

$(OBJDIR)/misc.o: src/misc.c $(OBJDIR)/ylog.o
	$(CC) $(CFLAGS) $< -c -o $@

$(OBJDIR)/fileop.o: src/fileop.c $(OBJDIR)/crc32.o $(OBJDIR)/md5.o $(OBJDIR)/sha256.o
	$(CC) $(CFLAGS) $< -c -o $@

$(OBJDIR)/filesend.o: src/filesend.c
	$(CC) $(CFLAGS) $< -c -o $@

$(OBJDIR)/conf.o: src/conf.c $(OBJDIR)/misc.o
	$(CC) $(CFLAGS) $< -c -o $@

$(OBJDIR)/main.o: src/main.c $(OBJDIR)/conf.o $(OBJDIR)/fileop.o $(OBJDIR)/filesend.o
	$(CC) $(CFLAGS) $< -c -o $@
	
$(OBJDIR)/main: $(OBJDIR)/main.o
	$(CC) $(OBJDIR)/crc32.o $(OBJDIR)/md5.o $(OBJDIR)/sha256.o $(OBJDIR)/ylog.o $(OBJDIR)/misc.o $(OBJDIR)/fileop.o $(OBJDIR)/filesend.o $(OBJDIR)/conf.o $(OBJDIR)/main.o -o $@ $(LDLIBS)
