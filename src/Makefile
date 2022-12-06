#Makefile


make: ftpclient.c ftpserver.c
	gcc -c -Wall -g aes.c
	gcc -c -Wall -g enc.c
	gcc -pthread -g -Wall ftpclient.c aes.o enc.o -o ftpclient
	mv ftpclient ../bin
	gcc -g -Wall ftpserver.c aes.o enc.o -o ftpserver
	mv ftpserver ../bin

clean:
	rm ../bin/ftpclient ../bin/ftpserver
