#Makefile

COMPRESSDEP = Alloc.c CpuArch.c LzFind.c LzFindMt.c LzFindOpt.c \
	LzmaDec.c LzmaEnc.c LzmaLib.c Threads.c comp.c

ENCRYPTDEP = enc.c aes.c

PARDEP = ftputil.c

make: ftpclient.c ftpserver.c $(COMPRESSDEP) $(ENCRYPTDEP) $(PARDEP) | bin
	gcc -g -Wall -lpthread ftpclient.c $(COMPRESSDEP) $(ENCRYPTDEP) $(PARDEP) -o ftpclient
	mv ftpclient ../bin/ftpclient/
	gcc -g -Wall -lpthread ftpserver.c $(COMPRESSDEP) $(ENCRYPTDEP) $(PARDEP) -o ftpserver
	mv ftpserver ../bin/ftpserver/

bin:
	mkdir -p ../bin
	mkdir -p ../bin/ftpclient/
	mkdir -p ../bin/ftpserver/
	
clean:
	rm ../bin/ftpclient ../bin/ftpserver
