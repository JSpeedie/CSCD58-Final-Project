# Acknowledgements

This CSCD58 Final Project is comprised of 3 major, distinct contributions.

## 1. Basic FTP Client/Server hosted by github user pranav93y
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
## 2. The 7-Zip LZMA SDK
[https://7-zip.org/sdk.html](https://7-zip.org/sdk.html)  
The compression algorithm used in our extension to FTP was the LZMA aka the
Lempel-Ziv-Markov chain algorithm, and the C implementation of it that our
project depends on was provided by this public domain SDK. To make it clear,
any .c or .h file that is not `ftpclient.c` or `ftpserver.c` came from this SDK,
and was not touched in anyway by us.
## 3. Us! The CSCD58 students who worked on this project!


# Description, Explanation, Goals

This project is a client/server program pair that implements FTP with 3
extensions: Parallelization, encryption, and compression. For this reason,
internally it is referred to as `pec-ftp`.

The goal was to implement all 3 extensions since we believed they each add
something meaningful to FTP.

## Why add parallelization?
FTP is traditionally sequential over a single data channel, with the file being
sent from the first byte to the last. We want to take advantage of multiple
data channels and send file segments simultaneously in order to significantly
speed up transfer, especially for large files.

## Why add encryption?
Last year Chrome and Firefox disabled support for FTP over security concerns
(see for instance
[https://chromestatus.com/feature/6246151319715840](https://chromestatus.com/feature/6246151319715840))
since it is entirely unencrypted (it has no security measures at all!) and
prone to a variety of attacks. We wanted to make our own modern file transfer
protocol that is secure!

## Why add compression?
To reduce the demands on the networks by using a high compression ratio
compression algorithm (such as the Lempel–Ziv–Markov chain algorithm (LZMA)
that we used in this project) and reduce the number of bytes the network has to
transport. It's worth noting that this incurs a cost to the server and client
who have to compress/uncompress at the beginning and end of every transfer
(which under most circumstances will increase the time it takes to transfer the
files), but they do so for the benefit of the network.

# Contributions

## Julian Speedie (speedie1, 1001712952)
Julian made the small fixes to the original FTP client/server program pair to
get it to work with all types of files, and worked on the compression part of
the project. Most of that work can be found in `pec-ftp.c`, `pec-ftp.h`, and
the compression related changes made in `do_put()`, `do_get()` of `src/ftpclient.c`
and in `do_retr()`, `do_stor()` of `src/ftpserver.c`.

Of note was the work on a custom file format needed for the proper transfer of
files from client to server or vice versa. This format which is used for files
that end in `.pec` (which hopefully you will never see since they are meant to
be temporary) takes the form of blocks which each have the structure:
```
8 bytes for a uint64_t representing the length (in bytes) of the compressed data in the data chunk of this block.
1 byte for a char representing whether the data in the data chunk is raw and unchanged (=0) or LZMA compressed (=1).
8 bytes for a uint64_t representing the length (in bytes) of the data when it is uncompressed.
x bytes of data (specified by the first 8 bytes) for the data chunk.
```
This was necessary for 2 reasons. First, the compression algorithm does not
always return a data stream that is smaller than the stream of original data.
In those cases, it's better to send the original data than data that not only
takes more space but must also be uncompressed upon receiving it. Secondly,
it's more efficient to send the size of the uncompressed data chunk (a number
we know from when we compressed the data chunk) and allocate precisely enough
space to uncompress than it is to allocate more than be needed and resizing
later.

## Dawson Brown (browndaw, 1005392932)
Dawson worked on the encryption part of the project.

## Jacky Fong (?, ?)
Jacky worked on the parallelization part of the project.


# Usage
In one terminal, start the server:
```
cd basic-FTP-Client-Server/
cd bin/ftpserver/
./ftpserver <listen-port>
```
For example:
```
./ftpserver 45678
```

In another terminal, start the client:
```
cd basic-FTP-Client-Server/
cd bin/ftpclient/
./ftpclient <server-ip> <server-listen-port>
```
For example: (make sure you run the server first)
```
./ftpclient 127.0.0.1 45678
```
You will interface with the server through the client, you cannot run any
commands on the server side, but you will see some output that might be helpful.


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
