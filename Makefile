CFLAGS=-Wall -march=native -O3
LDLIBS=-lsodium -ltoxcore
OBJDIR=build
CC=gcc

all: $(OBJDIR) $(OBJDIR)/main

clean:
	$(RM) -f $(OBJDIR)/main.o $(OBJDIR)/fileop.o $(OBJDIR)/filesend.o $(OBJDIR)/crc32.o $(OBJDIR)/md5.o $(OBJDIR)/sha256.o

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/crc32.o: src/crypto/crc32.c
	$(CC) $(CFLAGS) $< -c -o $@

$(OBJDIR)/md5.o: src/crypto/md5.c
	$(CC) $(CFLAGS) $< -c -o $@

$(OBJDIR)/sha256.o: src/crypto/sha256.c
	$(CC) $(CFLAGS) $< -c -o $@
	
crypto: $(OBJDIR)/crc32.o $(OBJDIR)/md5.o $(OBJDIR)/sha256.o

$(OBJDIR)/fileop.o: src/fileop.c crypto
	$(CC) $(CFLAGS) $< -c -o $@

$(OBJDIR)/filesend.o: src/filesend.c crypto
	$(CC) $(CFLAGS) $< -c -o $@
	
$(OBJDIR)/main.o: src/main.c $(OBJDIR)/fileop.o $(OBJDIR)/filesend.o
	$(CC) $(CFLAGS) $< -c -o $@
	
$(OBJDIR)/main: $(OBJDIR)/main.o $(OBJDIR)/fileop.o $(OBJDIR)/filesend.o $(OBJDIR)/crc32.o $(OBJDIR)/md5.o $(OBJDIR)/sha256.o
	$(CC) $(LDLIBS) $? -o $@
