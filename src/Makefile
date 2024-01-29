#Makefile
LIBS = -lpthread
CFLAGS = -Wall
CC = gcc -g
# CC = gcc

COMPRESSDEP = Alloc.c CpuArch.c LzFind.c LzFindMt.c LzFindOpt.c \
	LzmaDec.c LzmaEnc.c LzmaLib.c Threads.c comp.c
ENCRYPTDEP = enc.c aes.c
PARDEP = ftputil.c

make: ftpclient.c ftpserver.c $(COMPRESSDEP) $(ENCRYPTDEP) $(PARDEP) | bin
	$(CC) $(CFLAGS) $(LIBS) ftpclient.c $(COMPRESSDEP) $(ENCRYPTDEP) $(PARDEP) -o ftpclient
	mv ftpclient ../bin/ftpclient/
	$(CC) $(CFLAGS) $(LIBS) ftpserver.c $(COMPRESSDEP) $(ENCRYPTDEP) $(PARDEP) -o ftpserver
	mv ftpserver ../bin/ftpserver/

bin:
	mkdir -p ../bin
	mkdir -p ../bin/ftpclient/
	mkdir -p ../bin/ftpserver/

clean:
	rm ../bin/ftpclient ../bin/ftpserver
