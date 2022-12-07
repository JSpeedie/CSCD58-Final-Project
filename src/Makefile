#Makefile

COMPRESSDEP = Alloc.c CpuArch.c LzFind.c LzFindMt.c LzFindOpt.c \
	LzmaDec.c LzmaEnc.c LzmaLib.c Threads.c comp.c

ENCRYPTDEP = enc.c aes.c

make: ftpclient.c ftpserver.c $(COMPRESSDEP) $(ENCRYPTDEP) | bin
	gcc -pthread -g -Wall ftpclient.c $(COMPRESSDEP) $(ENCRYPTDEP) -o ftpclient
	mv ftpclient ../bin/ftpclient/
	gcc -g -Wall ftpserver.c $(COMPRESSDEP) $(ENCRYPTDEP) -o ftpserver
	mv ftpserver ../bin/ftpserver/

bin:
	mkdir -p ../bin
	mkdir -p ../bin/ftpclient/
	mkdir -p ../bin/ftpserver/
	
clean:
	rm ../bin/ftpclient ../bin/ftpserver
