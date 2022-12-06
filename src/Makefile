#Makefile

COMPRESSDEP = Alloc.c CpuArch.c LzFind.c LzFindMt.c LzFindOpt.c \
	LzmaDec.c LzmaEnc.c LzmaLib.c Threads.c pec-ftp.c

make: ftpclient.c ftpserver.c $(COMPRESSDEP)
	gcc -pthread -g -Wall ftpclient.c $(COMPRESSDEP) -o ftpclient
	mv ftpclient ../bin
	gcc -g -Wall ftpserver.c $(COMPRESSDEP) -o ftpserver
	mv ftpserver ../bin

clean:
	rm ../bin/ftpclient ../bin/ftpserver
