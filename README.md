# Acknowledgements

This CSCD58 Final Project was is comprised of 3 major, distinct contributions.

1. Basic FTP Client/Server hosted by github user pranav93y  
[https://github.com/pranav93y/Basic-FTP-Client-Server](https://github.com/pranav93y/Basic-FTP-Client-Server)  
It's worth noting that while it served at the FTP implementation that we made our
extensions on (as extensions to FTP was the focus of our project), this code
base was not fully functional. 2 small but important changes had to made
(and which are marked accordingly with comments) so that non-text files
could be both put and retrieved by a client. The original code only supports
text files and starts to break when it encounter non-terminal null bytes.
Instructions on how to use these programs found in this README are partially
modified (just for brevity, and to match other small changes we made (such as
removing an "-out" prefix from retrieved files) from the original instructions.
2. The 7-Zip LZMA SDK  
[https://7-zip.org/sdk.html](https://7-zip.org/sdk.html)  
The compression algorithm used in our extension to FTP was the LZMA aka the
Lempel-Ziv-Markov chain algorithm, and the C implementation of it that our
project depends on was provided by this public domain SDK. To make it clear,
any .c or .h file that is not `ftpclient.c` or `ftpserver.c` came from this SDK,
and was not touched in anyway by us.
3. Us! The CSCD58 students who worked on this project!


# Usage
- Server, in one terminal
```
cd basic-FTP-Client-Server/
cd bin/ftpserver/
./ftpserver <listen-port>
```
For example:
```
./ftpserver 45678
```

- Client, in another terminal
```
cd basic-FTP-Client-Server/
cd bin/ftpclient/
./ftpclient <server-ip> <server-listen-port>
```
For example: (make sure you run the server first)
```
./ftpclient 127.0.0.1 45678
```


# Client Interface Commands

- ls, lists the current directory
- get <filename>, gets the file from server to client. The file obtained will
  have and appended format of “-out” at the end of the original filename.
- put <filename>, puts the files from the client to the server. The file
  transferred to the server will have an appended format of “-out”, similar to
  get.
- quit, exits the client program

*NOTE: The server program will keep running even after a client has been
disconnected, waiting for future connections*

*NOTE: The provided client and server utilities can only access files within
the directories containing the executables, ls however, can list the contents
of directories contained within.*


# Example Run
- The console on the left is running the ftpclient, which currently does not
  contain other files in its directory.
- The console on the right is running the ftpserver, which contains the test
  files in its directory.
- The client firsts performs ls, prompting the server for a list of available
  files in its directory.
- The client then asks to get file "a", and quits.
- The file has been transferred from the server to the client, and has been
  renamed "a-out".

![Imgur](https://imgur.com/oyjYZ36.gif)
