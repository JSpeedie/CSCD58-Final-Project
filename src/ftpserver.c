#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "comp.h"
#include "enc.h"
#include "ftputil.h"
#include "pec-ftp.h"


int read_port_command(char *str, char *client_ip, uint16_t *client_port) {
	/* Read the port command, as specified at page 28 of:
	 * https://www.ietf.org/rfc/rfc959.txt */

	char *h1, *h2, *h3, *h4, *p1, *p2;
	uint16_t p1_i, p2_i;

	strtok(str, " ");
	h1 = strtok(NULL, ",");
	h2 = strtok(NULL, ",");
	h3 = strtok(NULL, ",");
	h4 = strtok(NULL, ",");
	p1 = strtok(NULL, ",");
	p2 = strtok(NULL, ",");

	sprintf(client_ip, "%s.%s.%s.%s", h1, h2, h3, h4);

	/* Reconstruct the port number from the 2 char inputs */
	p1_i = atoi(p1);
	p2_i = atoi(p2);
	(*client_port) = (p1_i << 8) + p2_i;

	return 0;
}


int setup_data_connection(int *fd, char *client_ip, int client_port, int server_port) {
	
	struct sockaddr_in client_addr, temp_addr;

	if ( ( (*fd) = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket error");
		return -1;
	}

	//bind port for data connection to be server port - 1 by using a temporary
	//struct sockaddr_in
	bzero(&temp_addr, sizeof(temp_addr));
	temp_addr.sin_family = AF_INET;
	temp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	/* Setup data connection to have a port that is the control connection
	 * port - 1, as specified at page 18 of:
	 * https://www.ietf.org/rfc/rfc959.txt */
	temp_addr.sin_port = htons(server_port - 1);

	while (bind( (*fd), (struct sockaddr*) &temp_addr, sizeof(temp_addr)) < 0) {
		server_port--;
		temp_addr.sin_port = htons(server_port);
	}

	/* Initiate data connection with client */
	bzero(&client_addr, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(client_port);
	if (inet_pton(AF_INET, client_ip, &client_addr.sin_addr) <= 0) {
		perror("inet_pton error");
		return -1;
	}

	if (connect(*fd, (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0) {
		perror("connect error");
		return -1;
	}

	return 1;
}


int get_filename(char *input, char *fileptr) {

	char *filename = NULL;
	filename = strtok(input, " ");
	filename = strtok(NULL, " ");

	if (filename == NULL) {
		return -1;
	} else {
		strncpy(fileptr, filename, strlen(filename));
		return 1;
	}
}


int get_command(char *command) {
	char cpy[1024];
	strcpy(cpy, command);
	char *str = strtok(cpy, " ");
	int value = 0;

	//populated value variable to indicate back to main which input was entered
    if(strcmp(str, "LIST") == 0){value = CMD_LS;}
    else if(strcmp(str, "RETR") == 0){value = CMD_GET;}
    else if(strcmp(str, "STOR") == 0){value = CMD_PUT;}
    else if(strcmp(str, "SKIP") == 0){value = 4;}
    else if(strcmp(str, "ABOR") == 0){value = 5;}

    return value;
}



/* CSCD58 addition - Encryption + Parallelization */
int do_dh_server(int controlfd, int datafd, uint32_t key[4]) {
	uint64_t dh_p = 1;
	dh_p = (dh_p << 32) - 99;
	uint64_t dh_g = 5;
	uint64_t dh_b, dh_ka, dh_kb, dh_k;
	// TODO: are readfds ever used properly in this function? we call select on
	// them, but no fd other than datafd is ever accessed and I don't think we
	// do anything with the fd_set readfds (i.e. we never check if any of the
	// fds in the set are set and never do anything particular to a given fd in
	// the set)
	fd_set readfds;
	FD_ZERO(&readfds);

	for (int i = 0; i < 4; i++) {
		dh_b = (rand() % (dh_p - 2)) + 2;
		dh_kb = sq_mp(dh_g, dh_b, dh_p);
		FD_SET(datafd, &readfds);

		select(datafd + 1, &readfds, NULL, NULL, NULL);
		read(datafd, (char *)&dh_ka, sizeof(dh_ka));

		write(datafd, (char *)&dh_kb, sizeof(dh_kb));

		dh_k = sq_mp(dh_ka, dh_b, dh_p);
		key[i] = dh_k & 0xffffffff;
	}
	
	return 0;
}
/* CSCD58 end of addition - Encryption + Parallelization */


int do_list(int controlfd, int datafd, char *input){
	char filelist[1024], sendline[MAXLINE+1], str[MAXLINE+1];
	bzero(filelist, (int)sizeof(filelist));

	if(get_filename(input, filelist) > 0){
		// TODO: what does it mean to have a filelist detected? did the user
		// specify a subdirectory?
		printf("(%d) Filelist Detected\n", getpid());
		sprintf(str, "ls %s", filelist);
		printf("(%d) Filelist: %s\n", getpid(), filelist);
		trim(filelist);
		//verify that given input is valid
		/*struct stat statbuf;
		stat(filelist, &statbuf);
		if(!(S_ISDIR(statbuf.st_mode))) {
			sprintf(sendline, "550 No Such File or Directory\n");
    		write(controlfd, sendline, strlen(sendline));
    		return -1;
		}*/
		// TODO: does this opendir() solution work? would stat() not be better?
    	DIR *dir = opendir(filelist);
    	if(!dir){
    		sprintf(sendline, "550 No Such File or Directory\n");
    		write(controlfd, sendline, strlen(sendline));
    		return -1;
		} else {
			closedir(dir);
		}
	} else {
		sprintf(str, "ls");
	}

	FILE *in;

	if (!(in = popen(str, "r"))) {
		sprintf(sendline, "451 Requested action aborted. Local error in processing.\n");
		write(controlfd, sendline, strlen(sendline));
		return -1;
	}

	while (fgets(sendline, MAXLINE, in) != NULL) {
		write(datafd, sendline, strlen(sendline));
		printf("%s", sendline);
		bzero(sendline, (int)sizeof(sendline));
	}

	sprintf(sendline, "200 Command OK");
	write(controlfd, sendline, strlen(sendline));
	pclose(in);

	return 1;
}


int do_retr(int controlfd, int *datafds, char *input) {
	char filename[1024], sendline[MAXLINE+1];
	bzero(filename, (int)sizeof(filename));
	bzero(sendline, (int)sizeof(sendline));
    fd_set writefdset;
    int maxfd;

	/* CSCD58 addition - Encryption */
	uint32_t key[4];
	do_dh_server(controlfd, datafds[0], key);
	/* CSCD58 end of addition - Encryption */

	if (get_filename(input, filename) > 0){
		if (access(filename, F_OK) != 0) {
			sprintf(sendline, "550 No Such File or Directory\n");
			write(controlfd, sendline, strlen(sendline));
			return -1;
		}
	} else {
		printf("Filename Not Detected\n");
		sprintf(sendline, "450 Requested file action not taken.\nFilename Not Detected\n");
		write(controlfd, sendline, strlen(sendline));
		return -1;
	}

	/* CSCD58 addition - Compression */
	char * prepared_fp;

	/* Encrypt (using key 'key') and compress the file stored at the
	 * filepath 'filename', outputting the result to the file at path
	 * 'prepared_fp' */
	if (0 != prepare_file(filename, key, &prepared_fp)) {
		fprintf(stderr, "ERROR: could not prepare the file!\n");
		sprintf(sendline, "451 Requested action aborted. Local error in processing\n");
		write(controlfd, sendline, strlen(sendline));
		return -1;
	}
	/* CSCD58 end of addition - Compression */
	
	maxfd = datafds[0];

	for (int i = 0; i < NDATAFD; i++) {
		if (datafds[i] > maxfd) {
			maxfd = datafds[i] + 1;
		}
	}

	FILE *in;

	if (!(in = fopen(prepared_fp, "rb"))) {
		sprintf(sendline, "451 Requested action aborted. Local error in processing\n");
		write(controlfd, sendline, strlen(sendline));
		return -1;
	}

	/* CSCD58 addition - Parallelization */
	uint16_t nmem_read = 0;
	uint32_t cur_pos = 0;
	int j = 0;
	/* the parallelized chunk header is one uint32_t (4 bytes) followed by one
	 * uint16_t (2 bytes) */
	int header_size = 6;
	
	while (0 != (nmem_read = fread(&sendline[header_size], 1, MAXLINE - header_size, in)) ) {
		FD_SETS(datafds, &writefdset, NDATAFD, j);
		// TODO: select() man page says if you're using select in a loop,
		// you gotta do some reinitializing. Make sure that takes place here
		// before the select() call
		// TODO: this should check for error? (i.e. when it returns -1)
		select(maxfd, NULL, &writefdset, NULL, NULL);
	
		for (int i = 0; i < NDATAFD; i++) {
			if (FD_ISSET(datafds[i], &writefdset)) {
				uint16_t to_write = nmem_read;
				/* Handle endianness */
				uint16_t nmem_read_n = htons(nmem_read);
				uint32_t cur_pos_n = htonl(cur_pos);
				// TODO: magic numbers here tied to sizeof nmem_read and cur_pos
				/* Fill in the header (aka. the first 6 bytes of sendline */
				memcpy(sendline, &cur_pos_n, sizeof(uint32_t));
				memcpy(&sendline[sizeof(uint32_t)], &nmem_read_n, sizeof(uint16_t));
				to_write += header_size;

				size_t written = 0;
				
				/* Write until all bytes in 'sendline' have been sent */
				while (written < to_write) {
					size_t nmem_written = write(datafds[i], &sendline[written], to_write - written); // TODO: Parallelize

					// TODO: is breaking the best way to handle an error here?
					if (nmem_written < 0) {
						break;
					} else {
						written += nmem_written;
					}
				}
				// TODO: is this bzero necessary? aren't we precise with how
				// many bytes we've read/how many we are to write?
				/* bzero(sendline, (size_t)sizeof(sendline)); */
				break;
			}
		}
		
		cur_pos += nmem_read;
	}
	/* CSCD58 end of addition - Parallelization */

	sprintf(sendline, "200 Command OK");
	write(controlfd, sendline, strlen(sendline));
	fclose(in);

	/* CSCD58 addition - Compression */
	if (KEEP_TEMP_ENC_FILES != 1) {
		if (0 != remove(prepared_fp)) {
			fprintf(stderr, "WARNING: could not remove temporary encrypted .enc file!\n");
		} 
	}

	/* Free dynamically allocated memory */
	free(prepared_fp);
	/* CSCD58 end of addition - Compression */

	return 1;
}


int do_stor(int controlfd, int datafd, char *input) {
	char filename[1024], sendline[MAXLINE+1], recvline[MAXLINE+1], str[MAXLINE+1];
	bzero(filename, (int)sizeof(filename));
	bzero(sendline, (int)sizeof(sendline));
	bzero(recvline, (int)sizeof(recvline));
	bzero(str, (int)sizeof(str));

	int n = 0, p = 0;

	/* CSCD58 addition - Parallelization */
	uint32_t key[4];
	do_dh_server(controlfd, datafd, key);
	/* CSCD58 end of addition - Parallelization */

	if(get_filename(input, filename) > 0){
		sprintf(str, "%s", filename);
	}else{
		printf("Filename Not Detected\n");
		sprintf(sendline, "450 Requested file action not taken.\n");
		write(controlfd, sendline, strlen(sendline));
		return -1;
	}

	/* CSCD58 addition - Compression */
	/* Make temporary name for receiving file
	 * ( '<filename>.comp.enc-XXXXXX' ) */
	char * recv_fp;
	if ( (recv_fp = temp_recv_name(filename)) == NULL) {
		fprintf(stderr, "ERROR: failed to receive file!\n");
		return -1;
	}
	/* CSCD58 end of addition - Compression*/

	FILE *fp;

	if ((fp = fopen(recv_fp, "w")) == NULL) {
		perror("file error");
		return -1;
	}

	while ((n = read(datafd, recvline, MAXLINE)) > 0) {
		// TODO: couldnt we clear the file and then open it in append mode to avoid this weird fseek call?
		fseek(fp, p, SEEK_SET);
		fwrite(recvline, 1, n, fp);
		p = p + n;
		//printf("%s", recvline);
		bzero(recvline, (int)sizeof(recvline));
	}

	sprintf(sendline, "200 Command OK");
	write(controlfd, sendline, strlen(sendline));
	fclose(fp);

	/* CSCD58 addition - Compression and Encryption */
	/* Decrypt (using key 'key') and decompress received file stored at the
	 * filepath 'recv_fp', outputting the result to the file at path
	 * 'filename' */
	if (process_received_file(filename, recv_fp, key) != 0) {
		fprintf(stderr, "ERROR: failed to process received file!\n");
		return -1;
	}

	/* Free dynamically allocated memory */
	free(recv_fp);
	/* CSCD58 end of addition - Compression */

	return 1;
}


int main(int argc, char **argv) {
	int listenfd, client_fd, port;
	struct sockaddr_in servaddr;
	pid_t pid;

	if (argc != 2) {
		printf("Invalid Number of Arguments...\n");
		printf("Usage: ./ftpserver <listen-port>\n");
		exit(-1);
	}

	/* Parse server port from commandline args */
	sscanf(argv[1], "%d", &port);

	if ( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket error");
		exit(-1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	if (bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
		perror("bind error");
		exit(-1);
	}

	if (listen(listenfd, LISTENQ) < 0) {
		perror("listen error");
		exit(-1);
	}

	struct sockaddr_in address;
	socklen_t addrlen = sizeof(address);

	while (1) {
		if ( (client_fd = accept(listenfd, (struct sockaddr *) &address, &addrlen)) < 0) {
			/* New client did NOT connect successfully */
			perror("accept error");
		/* New client connected successfully */
		} else {
			// TODO: not sure this prints the right address/port
			fprintf(stdout, "Notice: Received a new client %s:%d!\n", \
				inet_ntoa(address.sin_addr), ntohs(address.sin_port));
			pid = fork();
			if (pid < 0) {
				perror("fork error");
			/* If child process... */
			//child process---------------------------------------------------------------
			} else if (pid == 0) {
				close(listenfd);

				int datafds[NDATAFD], cmd, x = 0;
				uint16_t client_port = 0;
				char recvline[MAXLINE+1];
				char client_ip[INET_ADDRSTRLEN], command[4096];

				while (1) {
					bzero(recvline, (int)sizeof(recvline));
					bzero(command, (int)sizeof(command));

					/* Read a service(?) command from the client. The client is
					 * supposed to send a PORT service command first... */
					if ((x = read(client_fd, recvline, MAXLINE)) < 0){
						break;
					}

					printf("(%d) *****************\n", getpid());
					printf("(%d) %s\n", getpid(), recvline);

					/* ... but they could send a QUIT command first so check
					 * for that. */
					if (strncmp(recvline, "QUIT", 4) == 0) {
						printf("(%d) Quitting...\n", getpid());
						char msg[32];
						sprintf(msg,"221 Goodbye");
						write(client_fd, msg, strlen(msg));
						close(client_fd);
						break;
					}

					/* If the command was not to quit, then assume it is a PORT
					 * command */
					read_port_command(recvline, &client_ip[0], &client_port);
					printf("(%d) client_ip: %s client_port: %d\n", getpid(), client_ip, client_port);

					/* Setup all the data connections */
					for (int i = 0; i < NDATAFD; i++) {
						if (setup_data_connection(&datafds[i], client_ip, client_port, port) < 0) {
							break;
						}
					}

					/* After receiving the PORT command, we must handle a range
					 * of possible commands */
					if ((x = read(client_fd, command, MAXLINE)) < 0) {
						break;
					}

					printf("(%d) -----------------\n%s \n", getpid(), command);

					cmd = get_command(command);
					if(cmd == CMD_LS){
						do_list(client_fd, datafds[0], command);
					}else if(cmd == CMD_GET){
						do_retr(client_fd, datafds, command);
					}else if(cmd == CMD_PUT){
						do_stor(client_fd, datafds[0], command);
					// TODO: what's going on with this last 'else if'?
					}else if(cmd == 4){
						char reply[1024];
						sprintf(reply, "550 Filename Does Not Exist");
						write(client_fd, reply, strlen(reply));
					}
					close_data_connections(datafds);
				}
				// TODO: this print should almost certainly be removed
				printf("(%d) Exiting Child Process...\n", getpid());
				close(client_fd);
				_exit(1);
			}
			//end child process-------------------------------------------------------------
			close(client_fd);
		}
	}
}
