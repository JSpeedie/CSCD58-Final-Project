//myclient.c source file

#include    "LzmaLib.h"

#include 	<sys/types.h>
#include 	<sys/socket.h>
#include 	<strings.h>
#include	<string.h>
#include 	<arpa/inet.h>
#include 	<unistd.h>
#include 	<stdio.h>
#include    <ctype.h>
#include 	<stdlib.h>
#include 	<netinet/in.h>
#include	<stdbool.h>
#include 	<fcntl.h>
#include	<pthread.h>
#include    <sys/select.h>
#include    <sys/time.h>
#include    <sys/stat.h>


/* Change MAX_CHUNK value as needed depending on RAM constraints of the systems.
 * Max value is 4294967295 */
#define MAX_CHUNK 4294967295
#define PEC_HEADER_SIZE 5
#define CHUNK_HEADER_SIZE 4

#define 	MAXLINE 4096
#define     LISTENQ 1024
#define		TRUE	1
#define		FALSE	0

char GREEN[] = { "\x1B[32m" };
char RED[] = { "\x1B[31m" };
char NORMAL[] = { "\x1B[0m" };

//function trims leading and trailing whitespaces
void trim(char *str)
{

	int i;
    int begin = 0;

    int end = strlen(str) - 1;

    while (isspace((unsigned char) str[begin]))
        begin++;

    while ((end >= begin) && isspace((unsigned char) str[end]))
        end--;

    // Shift all characters back to the start of the string array.
    for (i = begin; i <= end; i++)
        str[i - begin] = str[i];

    str[i - begin] = '\0'; // Null terminate string.
}


int get_user_input(char *buffer){
	//clear buffer
    memset(buffer, 0, (int)sizeof(buffer));

    //print prompt
    printf("> ");

    //get user input
    if(fgets(buffer, 1024, stdin) == NULL){
            return -1;
    }

    return 1;
}

int get_port_string(char *str, char *ip, int n5, int n6){
    int i = 0;
    char ip_temp[1024];
    strcpy(ip_temp, ip);

    for(i = 0; i < strlen(ip); i++){
        if(ip_temp[i] == '.'){
            ip_temp[i] = ',';
        }
    }

    sprintf(str, "PORT %s,%d,%d", ip_temp, n5, n6);
    return 1;
}

int check_command(char *command){
	int i = 0, len = strlen(command), space = FALSE, count = 0;
	for(i = 0; i < len; i++){
		if(isspace(command[i]) == 0){
			space = FALSE;
			continue;
		}else{
			if(space == FALSE){
				count++;
			}
		}
	}

	if(count <= 1){return 1;}
	else{return -1;}
}

int get_command(char *command){
	int value, check = -1;
	char copy[1024];	
	while(check < 0){
    	char *str;
    	if(get_user_input(command) < 0){
    		printf("Cannot Read Command...\nPlease Try Again...\n");
            bzero(command, (int)sizeof(command));
    		continue;
    	}

        if(strlen(command) < 2){
            printf("No Input Detected...\nPlease Try Again\n");
            bzero(command, (int)sizeof(command));
            continue;
        }
        
        trim(command);
        strcpy(copy, command);

        if(check_command(copy) < 0){
            	printf("Invalid Format...\nPlease Try Again...\n");
            	bzero(command, (int)sizeof(command));
            	bzero(copy, (int)sizeof(copy));
            	continue;
        }
    	
    	
    	char delimit[]=" \t\r\n\v\f";
    	str = strtok(copy, delimit);
    	if((strcmp(str, "ls") == 0) || (strcmp(str, "get") == 0) || (strcmp(str, "put") == 0) || (strcmp(str, "quit") == 0)){
    		check = 1;


            //populated value valriable to indicate back to main which input was entered
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

int convert(uint16_t port, int *n5, int *n6){
    int i = 0;
    int x = 1;
    *n5 = 0;
    *n6 = 0;
    int temp = 0;
    for(i = 0; i< 8; i++){
        temp = port & x;
        *n6 = (*n6)|(temp);
        x = x << 1; 
    }

    port = port >> 8;
    x = 1;

    for(i = 8; i< 16; i++){
        temp = port & x;
        *n5 = ((*n5)|(temp));
        x = x << 1; 
    }
    return 1;
}

int get_ip_port(int fd, char *ip, int *port){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    getsockname(fd, (struct sockaddr*) &addr, &len);
    sprintf(ip, inet_ntoa(addr.sin_addr));
    *port = (uint16_t)ntohs(addr.sin_port);
    return 1;
}

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

int do_ls(int controlfd, int datafd, char *input){
    
    char filelist[256], str[MAXLINE+1], recvline[MAXLINE+1], *temp;
    bzero(filelist, (int)sizeof(filelist));
    bzero(recvline, (int)sizeof(recvline));
    bzero(str, (int)sizeof(str));

    fd_set rdset;
    int maxfdp1, data_finished = FALSE, control_finished = FALSE;

    if(get_filename(input, filelist) < 0){
        printf("No Filelist Detected...\n");
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
            printf("Server Data Response:\n");
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

int do_get(int controlfd, int datafd, char *input){
    char filename[256], str[MAXLINE+1], recvline[MAXLINE+1], *temp, temp1[1024];
    bzero(filename, (int)sizeof(filename));
    bzero(recvline, (int)sizeof(recvline));
    bzero(str, (int)sizeof(str));
    int n = 0, p = 0;

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
    }else{
        sprintf(str, "RETR %s", filename);
    }   
    printf("File: %s\n", filename);
    sprintf(temp1, "%s-out", filename);
    bzero(filename, (int)sizeof(filename));


    FD_ZERO(&rdset);
    FD_SET(controlfd, &rdset);
    FD_SET(datafd, &rdset);


    if(controlfd > datafd){
        maxfdp1 = controlfd + 1;
    }else{
        maxfdp1 = datafd + 1;
    }

    
    FILE *fp;
    if((fp = fopen(temp1, "w")) == NULL){
        perror("file error");
        return -1;
    }

    write(controlfd, str, strlen(str));
    while(1){
        if(control_finished == FALSE){FD_SET(controlfd, &rdset);}
        if(data_finished == FALSE){FD_SET(datafd, &rdset);}
        select(maxfdp1, &rdset, NULL, NULL, NULL);

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

        if(FD_ISSET(datafd, &rdset)){
            //printf("Server Data Response:\n");
            bzero(recvline, (int)sizeof(recvline));
            while((n = read(datafd, recvline, MAXLINE)) > 0){
                fseek(fp, p, SEEK_SET);
                fwrite(recvline, 1, n, fp);
                p = p + n;
                //printf("%s", recvline); 
                bzero(recvline, (int)sizeof(recvline)); 
            }
            data_finished = TRUE;
            FD_CLR(datafd, &rdset);
        }
        if((control_finished == TRUE) && (data_finished == TRUE)){
            break;
        }

    }
    bzero(filename, (int)sizeof(filename));
    bzero(recvline, (int)sizeof(recvline));
    bzero(str, (int)sizeof(str));
    fclose(fp);
    return 1;
}

int do_put(int controlfd, int datafd, char *input){
    char filename[256], str[MAXLINE+1], recvline[MAXLINE+1], sendline[MAXLINE+1], *temp, temp1[1024];
    bzero(filename, (int)sizeof(filename));
    bzero(recvline, (int)sizeof(recvline));
    bzero(str, (int)sizeof(str));
    //int n = 0, p = 0;

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
    }else{
        sprintf(str, "STOR %s", filename);
    }   

    sprintf(temp1, "cat %s", filename);
    bzero(filename, (int)sizeof(filename));


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
    extern FILE *popen();

    if (!(in = popen(temp1, "r"))) {
        printf("Cannot Run Command\nExiting...\n");
        return -1;
    }


    write(controlfd, str, strlen(str));
    while(1){
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
            //printf("Server Data Response:\n");
            while (fgets(sendline, MAXLINE, in) != NULL) {
                write(datafd, sendline, strlen(sendline));
                //printf("%s", sendline);
                bzero(sendline, (int)sizeof(sendline));
            }

            data_finished = TRUE;
            FD_CLR(datafd, &wrset);
            close(datafd);
        }
        if((control_finished == TRUE) && (data_finished == TRUE)){
            break;
        }
    }
    return 1;
}

unsigned char * read_file(char *filepath, uint64_t * ret_len, off_t start) {
	FILE *in_file = fopen(filepath, "rb");

	if (in_file == NULL) {
		perror("fopen (main)");
		return NULL;
	}

	if (start != 0) fseek(in_file, start, SEEK_SET);

	// TODO: maybe read file size with stat() and allocate so we don't
	// spend so much time on costly realloc's later
	unsigned int ret_size = 1024; /* Start by allocating 1kB */
	unsigned char * ret = calloc(ret_size, sizeof(char));
	/* Alloc call failed, exit */
	if (ret == NULL) {
		fprintf(stderr, "ERROR: read_file(): could not allocate buffer for file\n");
		return NULL;
	}
	unsigned int ret_i = 0;

	size_t nmem_read = 0;
	while (0 != (nmem_read = fread(&ret[ret_i], 1, ret_size - ret_i, in_file)) ) {
		ret_i += nmem_read;
		/* If there is no more space in the buffer, resize it */
		if (ret_i >= ret_size) {
			ret_size = (ret_size * 4) + 1;
			if (ret_size > MAX_CHUNK) ret_size = MAX_CHUNK;
			if (ret_i >= ret_size) {
				fprintf(stderr, "ERROR: read_file(): could not allocate buffer for file\n");
				return NULL;
			}
			ret = (unsigned char *) realloc(ret, ret_size);
			/* Alloc call failed, exit */
			if (ret == NULL) {
				fprintf(stderr, "ERROR: read_file(): could not allocate buffer for file\n");
				return NULL;
			}
		}
	}

	ret_i += nmem_read;
	*ret_len = ret_i;

	fclose(in_file);

	return ret;
}

int read_pecheader_file(unsigned char *pecheader, char *filepath) {
	FILE *in_file = fopen(filepath, "rb");

	if (in_file == NULL) {
		perror("fopen (main)");
		return -1;
	}

	if (PEC_HEADER_SIZE != fread(pecheader, 1, PEC_HEADER_SIZE, in_file) ) {
		fprintf(stderr, "ERROR: read_pecheader_file(): could not read header\n");
		return -1;
	}

	fclose(in_file);

	return 0;
}

int read_bytes(unsigned char *ret, size_t num_bytes, FILE *f) {
	size_t nmem_read = 0;
	if (num_bytes != (nmem_read = fread(ret, 1, num_bytes, f)) ) {
		fprintf(stderr, "ERROR: read_bytes(): could not read %lu bytes, was only able to read %lu\n", num_bytes, nmem_read);
		return -1;
	}

	return 0;
}

unsigned char * read_chunk_of_file(FILE * f, uint64_t * ret_len) {

	unsigned char * ret = malloc(MAX_CHUNK);
	/* Alloc call failed, exit */
	if (ret == NULL) {
		fprintf(stderr, "ERROR: read_chunk_of_file(): could not allocate buffer for file\n");
		return NULL;
	}

	size_t nmem_read = fread(&ret[0], 1, MAX_CHUNK, f);

	*ret_len = nmem_read;

	return ret;
}

int write_chunk_to_file(unsigned char *src, uint64_t src_len, FILE *f) {
	return fwrite(&src[0], 1, src_len, f);
}

int write_file(unsigned char *src, uint64_t src_len, char *filepath, char * file_opt) {
	FILE *out_file = fopen(filepath, file_opt);

	if (out_file == NULL) {
		perror("fopen (main)");
		return -1;
	}

	size_t increment = 1024; /* Write at most 1kB at a time */
	/* Don't write past the end of the buffer */
	if (increment > src_len) increment = src_len;
	size_t nmem_written = 0;
	uint64_t i = 0;
	while (i < src_len) {
		nmem_written = fwrite(&src[i], 1, increment, out_file);
		i += nmem_written;
		if (nmem_written != increment) break;
		/* Don't write past the end of the buffer */
		if (increment > src_len - i) increment = src_len - i;
	}

	fclose(out_file);

	return 0;
}

void clear_file(char* file) {

	FILE* victim = fopen(file, "w");
	if (victim == NULL) {
		perror("fopen (clear_file)");
		return;
	}
	fprintf(victim, "");
	fclose(victim);

	return;
}

int main(int argc, char **argv){
	if (argc != 2) {
		printf("Usage: ./ftpclient <filepath>\n");
		return -1;
	}

	char inputfile[1024];
	char * inputfilepath = &inputfile[0];
	sscanf(argv[1], "%s", inputfilepath);
	/* char * inputfilepath = "largefile.mkv"; */

	char * compressedoutputfilepath = (char *) malloc(strlen(inputfilepath) + 9 + 1);
	strncpy(compressedoutputfilepath, inputfilepath, strlen(inputfilepath));
	strncat(compressedoutputfilepath, "-out-comp", 9 + 1);
	char * uncompressedoutputfilepath = (char *) malloc(strlen(inputfilepath) + 9 + 7 + 1);
	strncpy(uncompressedoutputfilepath, inputfilepath, strlen(inputfilepath));
	strncat(uncompressedoutputfilepath, "-out-comp-uncomp", 9 + 7 + 1);

	struct stat s;
	stat(inputfilepath, &s);

	/* If the size of the file is too large to be done in one pass (> 2^32-1 bytes) */
	if (s.st_size > MAX_CHUNK) {
		fprintf(stderr, "WARNING: File is bigger than max chunk size and cannot be done in one pass. It must be compressed incrementally.\n");
	}

	FILE *in_file = fopen(inputfilepath, "rb");

	if (in_file == NULL) {
		perror("fopen (main)");
		return -1;
	}

	clear_file(compressedoutputfilepath);

	FILE *out_file = fopen(compressedoutputfilepath, "ab");

	if (out_file == NULL) {
		perror("fopen (main)");
		return -1;
	}

	off_t bytes_left = s.st_size;
	uint64_t inbuf_len;
	unsigned char *inbuf = NULL;

	/* Read through a file, chunk by chunk, compressing each chunk, preparing
	 * headers that describe the compressed chunk, and write the headers +
	 * the compressed chunk to the output file, repeating until it has read
	 * every byte of the input file */
	while (bytes_left > 0) {
		/* 1. Read the largest section we can */
		inbuf = read_chunk_of_file(in_file, &inbuf_len);
		if (inbuf == NULL) {
			fprintf(stderr, "ERROR: couldn't read input file\n");
			return -1;
		}
		bytes_left -= inbuf_len;

		uint64_t lzma_psize = LZMA_PROPS_SIZE;
		uint64_t dest_len = inbuf_len + inbuf_len / 3 + 128;
		uint64_t outbuf_len = LZMA_PROPS_SIZE + dest_len;
		unsigned char * outbuf = malloc(outbuf_len);
		/* Alloc call failed, exit */
		if (outbuf == NULL) {
			fprintf(stderr, "ERROR: could not allocate buffer for compressed file\n");
			return -1;
		}

		/* 2. Compress the data in 'inbuf' and put in 'outbuf' */
		MY_STDAPI compret = LzmaCompress( \
			&outbuf[LZMA_PROPS_SIZE], &dest_len, &inbuf[0], inbuf_len, \
			&outbuf[0], &lzma_psize, 5, 1 << 24, 3, 0, 2, 32, 1);
		if (compret != SZ_OK) {
			fprintf(stderr, "ERROR: compression call failed!\n");
			return -1;
		}
		outbuf_len = LZMA_PROPS_SIZE + dest_len;

		/* 3. Prepare the Chunk header */
		uint64_t chunk_len = outbuf_len;
		if (outbuf_len >= inbuf_len) {
			chunk_len = inbuf_len;
		}

		/* 4. Write the chunk header to the output file */
		/* If the compression did not reduce the size of the chunk */
		if (1 != fwrite(&chunk_len, sizeof(chunk_len), 1, out_file)) {
			fprintf(stderr, "ERROR: Could not write chunk length to output file\n");
		}

		/* 5. Prepare the PEC header */
		unsigned char pecheader[PEC_HEADER_SIZE];
		*((uint64_t *)&pecheader[1]) = inbuf_len;
		/* If the compression did not reduce the size of the chunk */
		if (outbuf_len >= inbuf_len) {
			pecheader[0] = 0;
		} else {
			pecheader[0] = 1;
		}

		/* 6. Write the pec header to the output file */
		if (1 != fwrite(&pecheader[0], PEC_HEADER_SIZE, 1, out_file)) {
			fprintf(stderr, "ERROR: Could not write PEC header to output file\n");
		}

		/* 7. Write the outbuf to the output file */
		/* If the compression did not reduce the size of the file */
		if (outbuf_len >= inbuf_len) {
			if (1 != fwrite(inbuf, inbuf_len, 1, out_file)) {
				fprintf(stderr, "ERROR: Could not write data content to output file\n");
			}
		} else {
			if (1 != fwrite(outbuf, outbuf_len, 1, out_file)) {
				fprintf(stderr, "ERROR: Could not write data content output file\n");
			}
		}
		/* Free dynamically alloc'd buffers */
		free(inbuf);
		free(outbuf);
	}

	fclose(in_file);
	fclose(out_file);





	printf("---------- begin uncompress\n");





	stat(compressedoutputfilepath, &s);

	in_file = fopen(compressedoutputfilepath, "rb");
	if (in_file == NULL) {
		perror("fopen (main)");
		return -1;
	}

	clear_file(uncompressedoutputfilepath);

	out_file = fopen(uncompressedoutputfilepath, "ab");
	if (out_file == NULL) {
		perror("fopen (main)");
		return -1;
	}

	bytes_left = s.st_size;
	inbuf = NULL;

	/* Read through a file, (chunk length header + pec header + chunk) by
	 * (chunk length header + pec header + chunk), uncompressing each chunk (if
	 * needed), and writing the uncompressed chunk (without any of the headers)
	 * to the output file. */
	while (bytes_left > 0) {
		/* 1. Read chunk length header*/
		uint64_t chunk_len;
		if (0 != read_bytes((unsigned char *)&chunk_len, sizeof(chunk_len), in_file)) {
			fprintf(stderr, "ERROR: couldn't read chunk header from chunk\n");
			return -1;
		}

		bytes_left -= sizeof(chunk_len);

		/* 2. Read PEC header for chunk */
		unsigned char pecheader[PEC_HEADER_SIZE];
		if (0 != read_bytes(&pecheader[0], PEC_HEADER_SIZE, in_file)) {
			fprintf(stderr, "ERROR: couldn't read pec header from chunk\n");
			return -1;
		}

		bytes_left -= PEC_HEADER_SIZE;

		/* 3. Read chunk */
		inbuf = calloc(chunk_len, sizeof(char));
		/* Alloc call failed, exit */
		if (inbuf == NULL) {
			fprintf(stderr, "ERROR: could not allocate buffer for chunk\n");
			return -1;
		}
		if (0 != read_bytes(inbuf, chunk_len, in_file) ) {
			fprintf(stderr, "ERROR: couldn't read chunk\n");
			return -1;
		}

		bytes_left -= chunk_len;

		/* 4. Process PEC header: If the compression wasn't used on this chunk... */
		if (pecheader[0] == 0) {
			/* ... just write chunk contents to the output file */
			if (1 != fwrite(inbuf, chunk_len, 1, out_file)) {
				fprintf(stderr, "ERROR: couldn't write output file\n");
				return -1;
			}
		} else {
			/* Decompress chunk, allocating pecheader[1] bytes for the output */
			uint64_t inbuf_len = chunk_len;
			uint64_t outbuf_len = *((uint64_t *)&pecheader[1]);
			unsigned char * outbuf = malloc(outbuf_len);
			/* Alloc call failed, exit */
			if (outbuf == NULL) {
				fprintf(stderr, "ERROR: could not allocate buffer for uncompressed file\n");
				return -1;
			}

			MY_STDAPI uncompret = LzmaUncompress( &outbuf[0], &outbuf_len, \
				&inbuf[LZMA_PROPS_SIZE], &inbuf_len, &inbuf[0], LZMA_PROPS_SIZE);
			if (uncompret != SZ_OK) {
				fprintf(stderr, "ERROR: uncompression call failed!\n");
				return -1;
			}

			/* Write the output to a file */
			if (1 != fwrite(outbuf, outbuf_len, 1, out_file)) {
				fprintf(stderr, "ERROR: couldn't write output file\n");
			}

			free(outbuf);
		}
		free(inbuf);
	}

	fclose(in_file);
	fclose(out_file);



	return 0;





	int server_port, controlfd, listenfd, datafd, code, n5, n6, x;
    uint16_t port;
	struct sockaddr_in servaddr, data_addr;
	char command[1024], ip[50], str[MAXLINE+1];


	if(argc != 3){
		printf("Invalid Number of Arguments...\n");
		printf("Usage: ./ftpclient <server-ip> <server-listen-port>\n");
		exit(-1);
	}

	//get server port
	sscanf(argv[2], "%d", &server_port);

    //set up control connection--------------------------------------------------
    if ( (controlfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	perror("socket error");
    	exit(-1);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(server_port);
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0){
    	perror("inet_pton error");
    	exit(-1);
    }

    if (connect(controlfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){
    	perror("connect error");
    	exit(-1);
    }


    //set up data connection------------------------------------------------------
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&data_addr, sizeof(data_addr));
    data_addr.sin_family      = AF_INET;
    data_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    data_addr.sin_port        = htons(0);

    bind(listenfd, (struct sockaddr*) &data_addr, sizeof(data_addr));

    listen(listenfd, LISTENQ);

    //get ip address from control port
    get_ip_port(controlfd, ip, (int *)&x);
    //x = 0;
    printf("x: %d\n", x);
    printf("ip: %s\n", ip);
    //get data connection port from listenfd
    get_ip_port(listenfd, str, (int *)&port);

    printf("Port: %d, str: %s\n",  port, str);
    convert(port, &n5, &n6);

    while(1){

        bzero(command, strlen(command));
        //get command from user
        code = get_command(command);

        //user has entered quit
        if(code == 4){
            char quit[1024];
            sprintf(quit, "QUIT");
            write(controlfd, quit, strlen(quit));
            bzero(quit, (int)sizeof(quit));
            read(controlfd,quit, 1024);
            printf("Server Response: %s\n", quit);
            break;
        }
        printf("command: %s\n", command);

        //send PORT n1,n2,n3,n4,n5,n6
        bzero(str, (int)sizeof(str));
        get_port_string(str, ip, n5, n6);

        write(controlfd, str, strlen(str));
        bzero(str, (int)sizeof(str));
        datafd = accept(listenfd, (struct sockaddr*)NULL, NULL);

        printf("Data connection Established...\n");

        if(code == 1){
            if(do_ls(controlfd, datafd, command) < 0){
                close(datafd);
                continue;
            }
        }else if(code == 2){
            if(do_get(controlfd, datafd, command) < 0){
                close(datafd);
                continue;
            }
        }else if(code == 3){
            if(do_put(controlfd, datafd, command) < 0){
                close(datafd);
                continue;
            }
        }
        close(datafd);
    }
    close(controlfd);
	return TRUE;
}
