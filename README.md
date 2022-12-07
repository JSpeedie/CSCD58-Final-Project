# Acknowledgements

This CSCD58 Final Project is comprised of 3 major, distinct contributions.

## 1. Basic FTP Client/Server by users pranav93y and nishant-sachdeva
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
the project. Most of that work can be found in `comp.c`, `comp.h`, and
the compression related changes made in `do_put()`, `do_get()` of `src/ftpclient.c`
and in `do_retr()`, `do_stor()` of `src/ftpserver.c`.

Of note was the work on a custom file format needed for the proper transfer of
files from client to server or vice versa. This format - which is used for files
that end in `.pec` (which hopefully you will never see since they are meant to
be temporary) - takes the form of repeated blocks, each having the structure:
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

## Jacky Fong (fongkwan, 991686118)
Jacky worked on the parallelization part of the project. Explored several different
I/O event notification facilities (e.g. POSIX select and poll) and third party libraries
(e.g. libuv and libev) for implementing parallelized file transfers. Evaluated the
different options to determine what works best with the chosen codebase.


# How to run and test
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
If your ftpclient says its connection was denied, make sure you entered the same
port for both your ftpserver and ftpclient. If you did, try a different port.

You will interface with the server through the client, you cannot run any
commands on the server side, but you will see some output that might be helpful.

To test, you may want to copy a variety of files into both your
`basic-FTP-Client-Server/bin/ftpserver/` directory (for the client to access) and
into your `basic-FTP-Client-Server/bin/ftpserver/` directory (for the client to
put). Once you've done that (which you can do while the client and server are
running), go to your terminal that is running `ftpclient` and begin any of the
following tests:

## 1. Client GET/Server RETR
1. From your ftpclient terminal, type `ls` at the prompt and hit enter to see
   what files the server has.
2. To retrieve a file from the server, type `get <filename>` and wait to
   receive the file.
3. It should appear (byte for byte identical to the original on the server, in
   content and name) in your `basic-FTP-Client-Server/bin/ftpclient` directory!

## 2. Client PUT/Server STOR
1. If you are running the ftpclient, press Ctrl+C to kill the process. From
   your `basic-FTP-Client-Server/bin/ftpclient` directory, run the shell
   command `ls` to see what files your client has access to. Then startup the
   client again with the command: `./ftpclient <server-ip>
   <server-listen-port>`
2. From your ftpclient terminal, type `ls` at the prompt and hit enter to see
   what files the server has (we don't want to overwrite any of them!)
3. To put a file from the server, type `get <local-filename>` (where
   `<local-filename>` is  one of the filenames printed by the shell command
   `ls` in step 1) and wait to receive the file.
4. It should appear (byte for byte identical to the original on the server, in
   content and name) in your `basic-FTP-Client-Server/bin/ftpclient` directory!

## TODO: show encryption? show parallelization? show compression? how? through wireshark?



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


![Imgur](https://imgur.com/oyjYZ36.gif)
