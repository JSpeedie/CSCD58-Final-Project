#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "comp.h"
#include "enc.h"
#include "ftputil.h"
#include "pec-ftp.h"


int get_user_input(char * buffer){
	//clear buffer
	memset(buffer, 0, (int) sizeof(buffer));

	//print the prompt
	printf("> ");

	// TODO: should this not loop? Currently it only ever reads a maximum of 1024 characters
	//get user input
	if (fgets(buffer, 1024, stdin) == NULL){
		return -1;
	}

	return 1;
}


// TODO: not sure if this function is endianness safe? I've cleaned it up,
// but the code is still the student code
/** Takes a port 'port' and modifies the uint8_ts at 'p1' and 'p2' such that the
 * former represents the 8 most significant bits of 'port' and the latter
 * represents the 8 least significant bits.
 * \param 'port' the port number.
 * \param '*p1' a pointer to a uint8_t that will be modified to contain the 8 most
 *     signifiant bits of 'port'.
 * \param '*p2' a pointer to a uint8_t that will be modified to contain the 8 least
 *     signifiant bits of 'port'.
 * \return void.
 */
void split_port(uint16_t port, uint8_t *p1, uint8_t *p2) {
	int x = 1;
	*p1 = 0;
	*p2 = 0;
	/* Set the 8 least significant bits of '(*p2)' to the
	   8 least significant bits of 'port' */
	for (int i = 0; i < 8; i++) {
		*p2 = (*p2)|(port & x);
		x = x << 1;
	}

	/* Set the 8 least significant bits of '(*p1)' to the
	   8 most significant bits of 'port' */
	port = port >> 8;
	x = 1;

	for (int i = 0; i < 8; i++) {
		*p1 = (*p1)|(port & x);
		x = x << 1;
	}
}


/* Takes a string representing an ip address and two ints representing the 8
 * least and 8 most significant bits in a port number and modifies 'str' to
 * contain a port command of the format specified by FTP.
 *
 * \param '*str' a string which will be modified to contain the PORT command.
 *     It must be at least 29 bytes long.
 * \param '*ip' a pointer to string representation of the ip address of the
 *     client.
 * \param 'p1' an int containing in its 8 least significant bits the 8 most
 *     significant bits of the port number.
 * \param 'p2' an int containing in its 8 least significant bits the 8 least
 *     significant bits of the port number.
 * \return void.
 */
void generate_port_command(char *str, char *ip, uint16_t port) {
	/* Generate the port command, as specified at page 28 of:
	 * https://www.ietf.org/rfc/rfc959.txt */

	int i = 0;
	char ip_temp[INET_ADDRSTRLEN];
	strncpy(ip_temp, ip, INET_ADDRSTRLEN);

	int ip_len = strlen(ip);

	for (i = 0; i < ip_len; i++){
		if (ip_temp[i] == '.'){
			ip_temp[i] = ',';
		}
	}

	uint8_t p1;
	uint8_t p2;
	split_port(port, &p1, &p2);

	sprintf(str, "PORT %s,%d,%d", ip_temp, p1, p2);
}


// TODO: what does this function do? my best guess right now
// is that it returns 1 if the number of whitespace characters in '*command'
// is <= 1.
int check_command(char *command){
	int len = strlen(command);
	int space = FALSE;
	int count = 0;

	for (int i = 0; i < len; i++) {
		if (isspace(command[i]) == 0) {
			space = FALSE; // TODO: space is first instantiated to FALSE and is never set to TRUE, so what is the point of this?
			continue; // TODO: is this continue necessary?
		} else {
			if (space == FALSE) { // TODO: space is first instantiated to FALSE and is never set to TRUE, so what is the point of this?
				count++;
			}
		}
	}

	if (count <= 1) {
		return 1;
	} else {
		return -1;
	}
}


// TODO: what does this function do? my best guess right now: ...
int get_command(char *command){
	int value, check = -1;
	char copy[1024];

	while (check < 0) {
		char *str;
		if (get_user_input(command) < 0) {
			printf("Cannot Read Command...\nPlease Try Again...\n");
			bzero(command, (int)sizeof(command));
			continue;
		}

		if (strlen(command) < 2) {
			printf("No Input Detected...\nPlease Try Again\n");
			bzero(command, (int)sizeof(command));
			continue;
		}

		trim(command);
		strcpy(copy, command);

		if (check_command(copy) < 0) {
			printf("Invalid Format...\nPlease Try Again...\n");
			bzero(command, (int)sizeof(command));
			bzero(copy, (int)sizeof(copy));
			continue;
		}

		char delimit[]=" \t\r\n\v\f";
		/* Get the first token from the command */
		str = strtok(copy, delimit);
		/* If that token is a valid FTP command */
		// TODO: these should be strncmp
		if ((strcmp(str, "ls") == 0) || (strcmp(str, "get") == 0) \
			|| (strcmp(str, "put") == 0) || (strcmp(str, "quit") == 0)) {

			check = 1;

			//populated value valriable to indicate back to main which input was entered
			// TODO: these should be strncmp
			if(strcmp(str, "ls") == 0){value = 1;}
			else if(strcmp(str, "get") == 0){value = 2;}
			else if(strcmp(str, "put") == 0){value = 3;}
			else if(strcmp(str, "quit") == 0){value = 4;}
		}else{
			printf("Incorrect Command Entered...\nPlease Try Again...\n");
			bzero(command, strlen(command));
			bzero(copy, sizeof(copy));
			continue;
		}
	}
	return value;
}


// TODO: clean up this function, add brief documentation
int get_filename(char *input, char *fileptr){
    char cpy[1024];
    char *filename = NULL;
    strcpy(cpy, input);
    trim(cpy);
    filename = strtok(cpy, " ");
    filename = strtok(NULL, " ");

    if(filename == NULL){
        fileptr = "\0";
        return -1;
    }else{
        strncpy(fileptr, filename, strlen(filename));
        return 1;
    }
}


/* CSCD58 Addition - Encryption */
int do_dh(int controlfd, int datafd, uint32_t key[4]) {
	uint64_t dh_p = 1;
	dh_p = (dh_p << 32) - 99;
	uint64_t dh_g = 5;
	uint64_t dh_a, dh_ka, dh_kb, dh_k;
	fd_set fds;
	FD_ZERO(&fds);

	for (int i = 0; i < 4; i++) {
		dh_a = (rand() % (dh_p - 2)) + 2;
		dh_ka = sq_mp(dh_g, dh_a, dh_p);
		FD_SET(datafd, &fds);

		write(datafd, (char *)&dh_ka, sizeof(dh_ka));

		select(datafd + 1, &fds, NULL, NULL, NULL);
		read(datafd, (char *)&dh_kb, sizeof(dh_kb));

		dh_k = sq_mp(dh_kb, dh_a, dh_p);
		key[i] = dh_k & 0xffffffff;
	}

	return 0;
}
/* End CSCD58 Addition - Encryption */


/** Perform the necessary operations to enact the QUIT FTP service command.
 *
 * \param 'controlfd' a file descriptor representing the control connection
 *     of the FTP.
 * \return 0 upon success, a negative int upon failure.
 */
int do_quit(int controlfd) {
	char quit[1024];
	sprintf(quit, "QUIT");
	int quit_len = strlen(quit);

	ssize_t nbytes = write(controlfd, quit, quit_len);
	if ((long long) nbytes < (long long) quit_len) {
		return -1;
	}

	bzero(quit, (int)sizeof(quit));
	// TODO: check for errors on read?
	read(controlfd, quit, 1024);
	printf("Server Response: %s\n", quit);

	return 0;
}


// TODO: possibly break up this function, add brief documentation
int do_ls(int controlfd, int datafd, char *input){

    char filelist[256], str[MAXLINE+1], recvline[MAXLINE+1], *temp;
    bzero(filelist, (int)sizeof(filelist));
    bzero(recvline, (int)sizeof(recvline));
    bzero(str, (int)sizeof(str));

    fd_set rdset;
    int maxfdp1, data_finished = FALSE, control_finished = FALSE;

    if(get_filename(input, filelist) < 0){
		if (DEBUG_OUTPUT == 1) {
        	fprintf(stdout, "No input filelist detected...\n");
		}
        sprintf(str, "LIST");
    }else{
        sprintf(str, "LIST %s", filelist);
    }

    bzero(filelist, (int)sizeof(filelist));

    FD_ZERO(&rdset);
    FD_SET(controlfd, &rdset);
    FD_SET(datafd, &rdset);

    if(controlfd > datafd){
        maxfdp1 = controlfd + 1;
    }else{
        maxfdp1 = datafd + 1;
    }

    write(controlfd, str, strlen(str));
    while(1){
        if(control_finished == FALSE){FD_SET(controlfd, &rdset);}
        if(data_finished == FALSE){FD_SET(datafd, &rdset);}
        select(maxfdp1, &rdset, NULL, NULL, NULL);

        if(FD_ISSET(controlfd, &rdset)){
            read(controlfd, recvline, MAXLINE);
            //strtok(recvline, " ");
            //recvline = strtok(NULL, " ");
			// TODO: I think this always prints the like, http code e.g. "200 Command OK"
            printf("%s\n", recvline);
            temp = strtok(recvline, " ");
            if(atoi(temp) != 200){
                printf("Exiting...\n");
                break;
            }
            control_finished = TRUE;
            bzero(recvline, (int)sizeof(recvline));
            FD_CLR(controlfd, &rdset);
        }

        if(FD_ISSET(datafd, &rdset)){
			if (DEBUG_OUTPUT == 1) {
				fprintf(stdout, "Server Data Response:\n");
			}
            while(read(datafd, recvline, MAXLINE) > 0){
                printf("%s", recvline);
                bzero(recvline, (int)sizeof(recvline));
            }

            data_finished = TRUE;
            FD_CLR(datafd, &rdset);
        }
        if((control_finished == TRUE) && (data_finished == TRUE)){
            break;
        }

    }
    bzero(filelist, (int)sizeof(filelist));
    bzero(recvline, (int)sizeof(recvline));
    bzero(str, (int)sizeof(str));
    return 1;
}


// TODO: break up this function, add brief documentation
int do_get(int controlfd, int *datafds, char *input) {
	char filename[256], serv_cmd[MAXLINE+1], recvline[MAXLINE+1], *temp;
	bzero(filename, (int)sizeof(filename));
	bzero(recvline, (int)sizeof(recvline));
	bzero(serv_cmd, (int)sizeof(serv_cmd));
	int n = 0;
	uint32_t p[NDATAFD];
	uint16_t rem[NDATAFD];
	uint8_t closed[NDATAFD];

	fd_set rdset;
	int maxfdp1, data_finished = FALSE, control_finished = FALSE;

	if(get_filename(input, filename) < 0){
		printf("No filename Detected...\n");
		char send[1024];
		sprintf(send, "SKIP");
		write(controlfd, send, strlen(send));
		bzero(send, (int)sizeof(send));
		read(controlfd, send, 1000);
		printf("Server Response: %s\n", send);
		return -1;
	}

	sprintf(serv_cmd, "RETR %s", filename);
	printf("File: %s\n", filename);

	/* CSCD58 addition - Compression */
	/* MTNRF: Make temporary name for receiving file
	 * ( '<filename>.comp.enc-XXXXXX' ) */
	char * c_out_fp_pure;
	char * recv_fp;
	/* MTNRF1: Get the "pure" name for the compressed version of the file,
	 * i.e., the temp compression file name minus the 'mkstemp()' digits 
	 * ( '<filename>.comp' ) */
	if ( (c_out_fp_pure = compression_name(filename)) == NULL) {
		fprintf(stderr, "ERROR: failed to receive file!\n");
		return -1;
	}

	/* MTNRF2: Take the "pure" compressed name and get a temp encryption name
	 * for it ( '<filename>.comp.enc-XXXXXX' ) */
	if ( (recv_fp = temp_encryption_name(c_out_fp_pure)) == NULL) {
		fprintf(stderr, "ERROR: failed to receive file!\n");
		return -1;
	}
	free(c_out_fp_pure);
	/* CSCD58 end of addition - Compression */

	FD_ZERO(&rdset);
	FD_SET(controlfd, &rdset);
	// FD_SET(datafd, &rdset);

    /* CSCD58 addition - Parallelization */
    int i = 0;
    maxfdp1 = datafds[0];
    for (i = 0; i < NDATAFD; i++) {
        FD_SET(datafds[i], &rdset);
        p[i] = 0;
        rem[i] = 0;
        closed[i] = 0;
        if (datafds[i] > maxfdp1)
            maxfdp1 = datafds[i] + 1;
    }
    /* End CSCD58 addition - Parallelization */

	if(controlfd > maxfdp1){
        maxfdp1 = controlfd + 1;
    }

	FILE *fp;
	if((fp = fopen(recv_fp, "w")) == NULL){
		perror("file error");
		return -1;
	}

	write(controlfd, serv_cmd, strlen(serv_cmd));

	/* CSCD58 Addition - Encryption */
	uint32_t key[4];
	do_dh(controlfd, datafds[0], key);
	/* End CSCD58 Addition - Encryption */

	int j = 0;
	int err = 0;

    /* CSCD58 Additon - Parallelization */
	while (1) {
        if(control_finished == FALSE){
			FD_SET(controlfd, &rdset);
		}
        //if(data_finished == FALSE){FD_SET(datafds, &rdset);}
        if(data_finished == FALSE){
			FD_SETS(datafds, &rdset, NDATAFD, i);
		}
        select(maxfdp1, &rdset, NULL, NULL, NULL);

        if(FD_ISSET(controlfd, &rdset)) {
            bzero(recvline, (int)sizeof(recvline));
            read(controlfd, recvline, MAXLINE);
            printf("Server Control Response: %s\n", recvline);
            temp = strtok(recvline, " ");
            if (atoi(temp) != 200) {
                err = 1;
                printf("File Error...\nExiting...\n");
                break;
            }
            control_finished = TRUE;
            bzero(recvline, (int)sizeof(recvline));
            FD_CLR(controlfd, &rdset);
        }

        for (i = 0; i< NDATAFD; i++) {
            if(FD_ISSET(datafds[i], &rdset)){
                bzero(recvline, (int)sizeof(recvline));

                if((n = recv(datafds[i], recvline, MAXLINE, MSG_PEEK)) > 0) {
                    if (n < 6) {
                        continue;
                    } else {
                        if (rem[i] > 0 && rem[i] < n) {
                            n = read(datafds[i], recvline, rem[i]);
                        } else {
                            n = read(datafds[i], recvline, n);
                        }
                    }

                    int off = 0;
                    if (rem[i] == 0) {
                        memcpy(p + i, recvline, 4);
                        memcpy(rem + i, recvline + 4, 2);
                        off += 6;
                    }
                    fseek(fp, p[i], SEEK_SET);
                    fwrite(recvline + off, 1, n - off, fp);
                    rem[i] = rem[i] - (n - off);
                    p[i] = p[i] + (n - off);
                    //printf("%s", recvline);
                    bzero(recvline, (int)sizeof(recvline));
                } else {
                    if (!closed[i]) {
                        j++;
                        closed[i] = 1;
                    }
                }
                if (j >= NDATAFD)
                    data_finished = TRUE;
                FD_CLR(datafds[i], &rdset);
            }
        }
        if((control_finished == TRUE) && (data_finished == TRUE)){
            break;
        }

    }
	fclose(fp);
    /* End CSCD58 Additoon - Parallelization */

	/* If there was an error receiving the file, delete the temp file it was to
	 * be written to */
	if (err) {
		if (0 != remove(recv_fp)) {
			fprintf(stderr, "WARNING: could not remove temporary file following an error!\n");
		}
		return -1;
	}

	/* CSCD58 addition - Compression */
	char * dec_out_fp;
	/* Make temporary name for compressed but decrypted file
	 * ( '<filename>.comp-XXXXXX' ) */
	if ( (dec_out_fp = temp_compression_name(filename)) == NULL) {
		fprintf(stderr, "ERROR: could not decrypt file!\n");
		return -1;
	}

	/* Decrypt content of file at 'recv_fp' writing output to file at
	 * 'dec_out_fp' */
	if (0 != dec_file(recv_fp, dec_out_fp, key)) {
		fprintf(stderr, "ERROR: could not decrypt file!\n");
		return -1;
	}

	if (KEEP_TEMP_ENC_FILES != 1) {
		if (0 != remove(recv_fp)) {
			fprintf(stderr, "WARNING: could not remove temporary encrypted .enc file!\n");
		}
	}

	/* Uncompress content of file at 'dec_out_fp' writing output to file at
	 * 'filename' */
	if (0 != uncomp_file(dec_out_fp, filename)) {
		fprintf(stderr, "ERROR: could not uncompress file!\n");
	}

	if (KEEP_TEMP_COMP_FILES != 1) {
		if (0 != remove(dec_out_fp)) {
			fprintf(stderr, "WARNING: could not remove temporary compressed .comp file!\n");
		}
	}

	/* Free dynamically allocated memory */
	free(dec_out_fp);
	free(recv_fp);
	/* CSCD58 end of addition - Compression */

	bzero(filename, (int)sizeof(filename));
	bzero(recvline, (int)sizeof(recvline));
	bzero(serv_cmd, (int)sizeof(serv_cmd));

	return 1;
}


// TODO: break up this function, add brief documentation
int do_put(int controlfd, int datafd, char *input){
	char filename[256], str[MAXLINE+1], recvline[MAXLINE+1], sendline[MAXLINE+1], serv_cmd[MAXLINE+1], *temp;
	bzero(filename, (int)sizeof(filename));
	bzero(recvline, (int)sizeof(recvline));
	bzero(str, (int)sizeof(str));

	fd_set wrset, rdset;
	int maxfdp1, data_finished = FALSE, control_finished = FALSE;

	if(get_filename(input, filename) < 0){
		printf("No filename Detected...\n");
		char send[1024];
		sprintf(send, "SKIP");
		write(controlfd, send, strlen(send));
		bzero(send, (int)sizeof(send));
		read(controlfd, send, 1000);
		printf("Server Control Response: %s\n", send);
		return -1;
	}

	sprintf(serv_cmd, "STOR %s", filename);

	/* CSCD58 addition - Compression */
	/* CF: Compress file */
	char * c_out_fp;

	/* CF1: Get temp name for compressed version of the file */
	if ( (c_out_fp = temp_compression_name(filename)) == NULL) {
		fprintf(stderr, "ERROR: could not compress file!\n");
		char send[1024];
		sprintf(serv_cmd, "SKIP");
		write(controlfd, serv_cmd, strlen(serv_cmd));
		bzero(send, (int)sizeof(send));
		read(controlfd, send, 1000);
		printf("Server Control Response: %s\n", send);
		return -1;
	}

	/* CF2: Compress content of file at 'filename' writing output to file at
	 * 'c_out_fp' */
	if (0 != comp_file(filename, c_out_fp)) {
		fprintf(stderr, "ERROR: could not compress file!\n");
		char send[1024];
		sprintf(serv_cmd, "SKIP");
		write(controlfd, serv_cmd, strlen(serv_cmd));
		bzero(send, (int)sizeof(send));
		read(controlfd, send, 1000);
		printf("Server Control Response: %s\n", send);
		return -1;
	}
	/* CSCD58 end of addition - Compression */

	FD_ZERO(&wrset);
	FD_ZERO(&rdset);
	FD_SET(controlfd, &rdset);
	FD_SET(datafd, &wrset);

	if(controlfd > datafd){
		maxfdp1 = controlfd + 1;
	}else{
		maxfdp1 = datafd + 1;
	}

	FILE *in;

	write(controlfd, serv_cmd, strlen(serv_cmd));

	/* CSCD58 Addition - Encryption + Compression */
	uint32_t key[4];
	do_dh(controlfd, datafd, key);

	/* EF: Encrypt file */
	char * c_out_fp_pure;
	char * e_out_fp;

	/* EF3: Get the temp name for the compressed, encrypted version of the file */
	/* EF3a: Get the "pure" name for the compressed version of the file, i.e.,
	 * the temp compression file name minus the 'mkstemp()' digits */
	if ( (c_out_fp_pure = compression_name(filename)) == NULL) {
		fprintf(stderr, "ERROR: could not encrypt file!\n");
		char send[1024];
		sprintf(serv_cmd, "SKIP");
		write(controlfd, serv_cmd, strlen(serv_cmd));
		bzero(send, (int)sizeof(send));
		read(controlfd, send, 1000);
		printf("Server Control Response: %s\n", send);
		return -1;
	}
	/* EF3b: Get temp name for encrypted version of the compressed file */
	if ( (e_out_fp = temp_encryption_name(c_out_fp_pure)) == NULL) {
		fprintf(stderr, "ERROR: could not encrypt file!\n");
		char send[1024];
		sprintf(serv_cmd, "SKIP");
		write(controlfd, serv_cmd, strlen(serv_cmd));
		bzero(send, (int)sizeof(send));
		read(controlfd, send, 1000);
		printf("Server Control Response: %s\n", send);
		return -1;
	}

	/* EF4: Encrypt content of file at 'filename' writing output to file at
	 * 'e_out_fp' */
	if (0 != enc_file(c_out_fp, e_out_fp, key)) {
		fprintf(stderr, "ERROR: could not encrypt file!\n");
		char send[1024];
		sprintf(serv_cmd, "SKIP");
		write(controlfd, serv_cmd, strlen(serv_cmd));
		bzero(send, (int)sizeof(send));
		read(controlfd, send, 1000);
		printf("Server Control Response: %s\n", send);
		return -1;
	}
	/* End CSCD58 Addition - Encryption + Compression */

	if ((in = fopen(e_out_fp, "rb")) == NULL) {
		printf("Cannot Run Command\nExiting...\n");
		return -1;
	}

	while (1) {
		if(control_finished == FALSE){FD_SET(controlfd, &rdset);}
		if(data_finished == FALSE){FD_SET(datafd, &wrset);}
		select(maxfdp1, &rdset, &wrset, NULL, NULL);

		if(FD_ISSET(controlfd, &rdset)){
			bzero(recvline, (int)sizeof(recvline));
			read(controlfd, recvline, MAXLINE);
			printf("Server Control Response: %s\n", recvline);
			temp = strtok(recvline, " ");
			if(atoi(temp) != 200){
				printf("File Error...\nExiting...\n");
				break;
			}
			control_finished = TRUE;
			bzero(recvline, (int)sizeof(recvline));
			FD_CLR(controlfd, &rdset);
		}

		if(FD_ISSET(datafd, &wrset)){
			bzero(sendline, (int)sizeof(sendline));
			/* CSCD58 addition */
			size_t nmem_read = 0;
			while (0 != (nmem_read = fread(sendline, 1, sizeof(sendline), in)) ) {
				write(datafd, sendline, nmem_read);
				bzero(sendline, (size_t)sizeof(sendline));
			}
			/* CSCD58 end of addition */

			data_finished = TRUE;
			FD_CLR(datafd, &wrset);
			close(datafd);
		}
		if((control_finished == TRUE) && (data_finished == TRUE)){
			break;
		}
	}
	/* CSCD58 addition - Compression */
	if (KEEP_TEMP_COMP_FILES != 1) {
		if (0 != remove(c_out_fp)) {
			fprintf(stderr, "WARNING: could not remove temporary compressed .comp file!\n");
		}
	}
	if (KEEP_TEMP_ENC_FILES != 1) {
		if (0 != remove(e_out_fp)) {
			fprintf(stderr, "WARNING: could not remove temporary encrypted .enc file!\n");
		}
	}

	/* Close open files */
	fclose(in);

	/* Free dynamically allocated memory */
	free(c_out_fp);
	free(c_out_fp_pure);
	free(e_out_fp);
	/* CSCD58 end of addition - Compression */
	return 1;
}


/* Set up control connection */
int setup_control_conn(int * controlfd, struct sockaddr_in * serv_addr, \
	char * ip_addr, uint16_t port) {

	/* Create a control socket */
	if ( ( (*controlfd) = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket error");
		return -1;
	}

	/* Create the configuration for the control connection */
	bzero(serv_addr, sizeof( (*serv_addr) ));
	serv_addr->sin_family = AF_INET;
	serv_addr->sin_port = htons(port);
	if (inet_pton(AF_INET, ip_addr, &(serv_addr->sin_addr)) <= 0) {
		return -2;
	}

	/* Connect the control socket to the server (using the configuration) */
	if (connect( (*controlfd), (struct sockaddr *) serv_addr, sizeof( (*serv_addr) )) < 0) {
		return -3;
	}

	return 0;
}


/* Set up data connection */
int setup_data_conn(int * listenfd, struct sockaddr_in * data_addr, \
	uint16_t port) {

	/* Create a data socket for listening */
	if (( (*listenfd) = socket(AF_INET, SOCK_STREAM, 0) ) < 0) {
		return -1;
	}

	/* Create the configuration for the data connection */
	bzero(data_addr, sizeof( (*data_addr) ));
	data_addr->sin_family = AF_INET;
	data_addr->sin_addr.s_addr = htonl(INADDR_ANY);
	data_addr->sin_port = htons(port);

	/* Bind the data socket (apply configuration) */
	if (bind( (*listenfd), (struct sockaddr *) data_addr, sizeof( (*data_addr) )) < 0) {
		return -2;
	}

	/* Set the socket as passive (one that listens) with a backlog of
	 * 'LISTENQ' */
	if ( (listen( (*listenfd), LISTENQ)) < 0) {
		return -3;
	}

	return 0;
}


int main(int argc, char **argv) {
	int server_port, controlfd, listenfd, datafds[NDATAFD], cmd;
	struct sockaddr_in serv_addr, data_addr;
	char command[1024], ip[INET_ADDRSTRLEN], port_command[MAXLINE+1];

	if (argc != 3) {
		printf("Invalid Number of Arguments...\n");
		printf("Usage: ./ftpclient <server-ip> <server-listen-port>\n");
		exit(-1);
	}

	/* Parse server address and port from commandline args */
	strncpy(ip, argv[1], INET_ADDRSTRLEN);
	sscanf(argv[2], "%d", &server_port);

	/* Setup control and data connections, as required by the FTP at page 8 of:
	 * https://www.ietf.org/rfc/rfc959.txt */
	if (setup_control_conn(&controlfd, &serv_addr, argv[1], server_port) < 0) {
		perror("control connection setup error");
		exit(-1);
	}
	// TODO: should the port be 0?
	if (setup_data_conn(&listenfd, &data_addr, 0) < 0) {
		perror("data connection setup error");
		exit(-1);
	}

	/* Get the port through which the client communicating */
	uint16_t client_conn_port = 0;
	if (get_port(listenfd, &client_conn_port) < 0) {
		perror("data connection setup error");
		exit(-1);
	}
	printf("IP: %s, Port: %d\n", ip, client_conn_port);
	/* Produce the port command of structure: "PORT h1,h2,h3,h4,p1,p2" */
	bzero(port_command, (int) sizeof(port_command));
	generate_port_command(port_command, ip, client_conn_port);

	while (1) {
		bzero(command, strlen(command));
		//get command from user
		cmd = get_command(command);

		/* If the user entered the "quit" command at the prompt */
		if (cmd == CMD_QUIT){
			// TODO: double check this if statement. What is it checking for?
			// In which (if any) circumstances should the program then
			// close_data_connections()?
			if (do_quit(controlfd) < 0) {
				close_data_connections(datafds);
			}
			break;
		}

		if (DEBUG_OUTPUT == 1) {
			fprintf(stdout, "command: %s\n", command);
		}

		/* Send the port command that was constructed earlier */
		write(controlfd, port_command, strlen(port_command));

        /* CSCD58 Addition - Parallelization */
        int i = 0;
        for (i = 0; i < NDATAFD; i++) {
            datafds[i] = accept(listenfd, (struct sockaddr*)NULL, NULL);
            //printf("%d-th data connection established\n", i);
        }
        /* End CSCD58 Addition - Parallelization */

		if (DEBUG_OUTPUT == 1) {
			fprintf(stdout, "Data connection established. Ready to receive data!\n");
		}

		if (cmd == CMD_LS) {
			if (do_ls(controlfd, datafds[0], command) < 0) {
				close_data_connections(datafds);
				continue;
			}
		} else if (cmd == CMD_GET) {
			if (do_get(controlfd, datafds, command) < 0) {
				close_data_connections(datafds);
				continue;
			}
		} else if(cmd == CMD_PUT) {
			if (do_put(controlfd, datafds[0], command) < 0) {
				close_data_connections(datafds);
				continue;
			}
		}
		/* CSCD58 Addition - Parallelization? */
		close_data_connections(datafds);
		/* End CSCD58 Addition - Parallelization? */
	}
	close(controlfd);
	return TRUE;
}
