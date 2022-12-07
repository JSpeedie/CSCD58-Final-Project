#include "comp.h"
#include "enc.h"
#include "ftputil.h"
#include "pec-ftp.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <time.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

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

int get_client_ip_port(char *str, char *client_ip, int *client_port){
	char *n1, *n2, *n3, *n4, *n5, *n6;
	int x5, x6;

	strtok(str, " ");
	n1 = strtok(NULL, ",");
	n2 = strtok(NULL, ",");
	n3 = strtok(NULL, ",");
	n4 = strtok(NULL, ",");
	n5 = strtok(NULL, ",");
	n6 = strtok(NULL, ",");

	sprintf(client_ip, "%s.%s.%s.%s", n1, n2, n3, n4);

	x5 = atoi(n5);
	x6 = atoi(n6);
	*client_port = (256*x5)+x6;

	printf("client_ip: %s client_port: %d\n", client_ip, *client_port);
	return 1;
}

int setup_data_connection(int *fd, char *client_ip, int client_port, int server_port){
	
    struct sockaddr_in cliaddr, tempaddr;

    if ( (*fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	perror("socket error");
    	return -1;
    }

    //bind port for data connection to be server port - 1 by using a temporary struct sockaddr_in
    bzero(&tempaddr, sizeof(tempaddr));
    tempaddr.sin_family = AF_INET;
    tempaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    tempaddr.sin_port   = htons(server_port-1);

    while((bind(*fd, (struct sockaddr*) &tempaddr, sizeof(tempaddr))) < 0){
    	//perror("bind error");
        server_port--;
    	tempaddr.sin_port   = htons(server_port);
    }


    //initiate data connection fd with client ip and client port             
    bzero(&cliaddr, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port   = htons(client_port);
    if (inet_pton(AF_INET, client_ip, &cliaddr.sin_addr) <= 0){
    	perror("inet_pton error");
    	return -1;
    }

    if (connect(*fd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0){
    	perror("connect error");
    	return -1;
    }

    return 1;
}

int get_filename(char *input, char *fileptr){

    char *filename = NULL;
    filename = strtok(input, " ");
    filename = strtok(NULL, " ");
    if(filename == NULL){
        return -1;
    }else{
    	strncpy(fileptr, filename, strlen(filename));
        return 1;
    }
}

int get_command(char *command){
	char cpy[1024];
	strcpy(cpy, command);
	char *str = strtok(cpy, " ");
	int value;

	//populated value valriable to indicate back to main which input was entered
    if(strcmp(str, "LIST") == 0){value = 1;}
    else if(strcmp(str, "RETR") == 0){value = 2;}
    else if(strcmp(str, "STOR") == 0){value = 3;}
    else if(strcmp(str, "SKIP") == 0){value = 4;}
    else if(strcmp(str, "ABOR") == 0){value = 5;}

    return value;
}

int do_dh(int controlfd, int datafd, uint32_t key[4]) {
    uint64_t dh_p = 1;
    dh_p = (dh_p << 32) - 99;
    uint64_t dh_g = 5;
    uint64_t dh_b, dh_ka, dh_kb, dh_k;
    fd_set fds;
    FD_ZERO(&fds);

    for (int i = 0; i < 4; i++) {
        dh_b = (rand() % (dh_p - 2)) + 2;
        dh_kb = sq_mp(dh_g, dh_b, dh_p);
        FD_SET(datafd, &fds);

        select(datafd + 1, &fds, NULL, NULL, NULL);
        read(datafd, (char *)&dh_ka, sizeof(dh_ka));
        
        write(datafd, (char *)&dh_kb, sizeof(dh_kb));

        dh_k = sq_mp(dh_ka, dh_b, dh_p);
        key[i] = dh_k & 0xffffffff;
    }
    
    return 0;
}

int do_list(int controlfd, int datafd, char *input){
	char filelist[1024], sendline[MAXLINE+1], str[MAXLINE+1];
	bzero(filelist, (int)sizeof(filelist));

	if(get_filename(input, filelist) > 0){
		printf("Filelist Detected\n");
		sprintf(str, "ls %s", filelist);
		printf("Filelist: %s\n", filelist);
		trim(filelist);
		//verify that given input is valid
		/*struct stat statbuf;
		stat(filelist, &statbuf);
		if(!(S_ISDIR(statbuf.st_mode))) {
			sprintf(sendline, "550 No Such File or Directory\n");
    		write(controlfd, sendline, strlen(sendline));
    		return -1;
		}*/
    	DIR *dir = opendir(filelist);
    	if(!dir){
    		sprintf(sendline, "550 No Such File or Directory\n");
    		write(controlfd, sendline, strlen(sendline));
    		return -1;
    	}else{closedir(dir);}

	}else{
		sprintf(str, "ls");
	}

	 //initiate file pointer for popen()
    FILE *in;
    extern FILE *popen();

    if (!(in = popen(str, "r"))) {
    	sprintf(sendline, "451 Requested action aborted. Local error in processing\n");
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

int do_retr(int controlfd, int *datafds, char *input){
	char filename[1024], sendline[MAXLINE+1], str[MAXLINE+1];
	bzero(filename, (int)sizeof(filename));
	bzero(sendline, (int)sizeof(sendline));
	bzero(str, (int)sizeof(str));

    uint32_t key[4];
    do_dh(controlfd, datafds[0], key);

	if(get_filename(input, filename) > 0){
		sprintf(str, "cat %s", filename);

		if((access(filename, F_OK)) != 0){
			sprintf(sendline, "550 No Such File or Directory\n");
			write(controlfd, sendline, strlen(sendline));
			return -1;
		}
	}else{
		printf("Filename Not Detected\n");
		sprintf(sendline, "450 Requested file action not taken.\nFilename Not Detected\n");
		write(controlfd, sendline, strlen(sendline));
		return -1;
	}

	/* CSCD58 addition */
	char * encsuffix = ".enc";
	int encsuffix_len = strlen(encsuffix);
	char * compsuffix = ".comp";
	int compsuffix_len = strlen(compsuffix);
	int compoutputfilepathlen = strlen(filename) + compsuffix_len + 7 + 1;
	char compoutputfilepath[compoutputfilepathlen];
	bzero(compoutputfilepath, compoutputfilepathlen);
	strncpy(&compoutputfilepath[0], filename, strlen(filename));
	strncat(&compoutputfilepath[0], compsuffix, compsuffix_len + 1);
	/* Generate a temp name for our compressed .comp file */
	strncat(&compoutputfilepath[0], "-XXXXXX", 8);
	int r = mkstemp(compoutputfilepath);
	close(r);
	unlink(compoutputfilepath);

	if (0 != comp_file(filename, compoutputfilepath)) {
		fprintf(stderr, "ERROR: could not compress file!\n");
	}
	sprintf(str, "cat %s", compoutputfilepath);

	int encoutputfilepathlen = strlen(filename) + encsuffix_len + 7 + 1;
	char encoutputfilepath[encoutputfilepathlen];
	bzero(encoutputfilepath, encoutputfilepathlen);
	strncpy(&encoutputfilepath[0], filename, strlen(filename));
	strncat(&encoutputfilepath[0], encsuffix, encsuffix_len + 1);
	/* Generate a temp name for our encrypted .enc file */
	strncat(&encoutputfilepath[0], "-XXXXXX", 8);
	r = mkstemp(encoutputfilepath);
	close(r);
	unlink(encoutputfilepath);

	enc_file(str, encoutputfilepath, key);

	sprintf(str, "cat %s", encoutputfilepath);
	/* CSCD58 end of addition */

	FILE *in;
	extern FILE *popen();

	if (!(in = popen(str, "r"))) {
		sprintf(sendline, "451 Requested action aborted. Local error in processing\n");
		write(controlfd, sendline, strlen(sendline));
		return -1;
	}

	/* CSCD58 addition */
	size_t nmem_read = 0;
    int i = 0;
	while (0 != (nmem_read = fread(sendline, 1, sizeof(sendline), in)) ) {
		write(datafds[i], sendline, nmem_read); // TODO: Parallelize
		bzero(sendline, (size_t)sizeof(sendline));
        i = (i + 1 ) %  NDATAFD;
	}
	/* CSCD58 end of addition */

	sprintf(sendline, "200 Command OK");
	write(controlfd, sendline, strlen(sendline));
	pclose(in);
	/* CSCD58 addition */
	if (KEEP_TEMP_ENC_FILES != 1) {
		if (0 != remove(encoutputfilepath)) {
			fprintf(stderr, "WARNING: could not remove temporary encrypted .enc file!\n");
		} 
	}
	if (KEEP_TEMP_COMP_FILES != 1) {
		if (0 != remove(compoutputfilepath)) {
			fprintf(stderr, "WARNING: could not remove temporary compressed .comp file!\n");
		} 
	}
	/* CSCD58 end of addition */
	return 1;
}

int do_stor(int controlfd, int datafd, char *input){
	char filename[1024], sendline[MAXLINE+1], recvline[MAXLINE+1], str[MAXLINE+1], temp1[1024];
	bzero(filename, (int)sizeof(filename));
	bzero(sendline, (int)sizeof(sendline));
	bzero(recvline, (int)sizeof(recvline));
	bzero(str, (int)sizeof(str));

	int n = 0, p = 0;
	
	uint32_t key[4];
	do_dh(controlfd, datafd, key);

	if(get_filename(input, filename) > 0){
		sprintf(str, "%s", filename);
	}else{
		printf("Filename Not Detected\n");
		sprintf(sendline, "450 Requested file action not taken.\n");
		write(controlfd, sendline, strlen(sendline));
		return -1;
	}

	/* CSCD58 addition */
	/* Make temporary <filename>.comp.enc-XXXXXX file for receiving the file */
	char * encsuffix = ".enc";
	int encsuffix_len = strlen(encsuffix);
	char * compsuffix = ".comp";
	int compsuffix_len = strlen(compsuffix);
	int recvfilepathlen = strlen(filename) + compsuffix_len + encsuffix_len + 7 + 1;
	char recvfilepath[recvfilepathlen];
	bzero(recvfilepath, recvfilepathlen);
	strncpy(&recvfilepath[0], filename, strlen(filename));
	strncat(&recvfilepath[0], compsuffix, compsuffix_len + 1);
	strncat(&recvfilepath[0], encsuffix, encsuffix_len + 1);
	/* Generate a temp name for our compressed .comp file */
	strncat(&recvfilepath[0], "-XXXXXX", 8);
	int r = mkstemp(recvfilepath);
	close(r);
	unlink(recvfilepath);
	sprintf(temp1, "%s", recvfilepath);
	/* sprintf(temp1, "%s-out", filename); */
	/* CSCD58 end of addition */

	FILE *fp;
	if((fp = fopen(temp1, "w")) == NULL){
		perror("file error");
		return -1;
	}

	while((n = read(datafd, recvline, MAXLINE)) > 0){
		fseek(fp, p, SEEK_SET);
		fwrite(recvline, 1, n, fp);
		p = p + n;
		//printf("%s", recvline); 
		bzero(recvline, (int)sizeof(recvline)); 
	}

	sprintf(sendline, "200 Command OK");
	write(controlfd, sendline, strlen(sendline));
	fclose(fp);

	/* CSCD58 addition */
	/* Create 'cat <filename>.comp.enc-XXXXXX' */
	int unencinputfilelen = strlen(temp1) + 5;
	char unencinputfilepath[unencinputfilelen];
	bzero(unencinputfilepath, unencinputfilelen);
	sprintf(unencinputfilepath, "cat %s", temp1);
	/* temp1 = <filename>.comp.enc-XXXXXX */
	int unencoutputfilepathlen = strlen(temp1) - 7 - encsuffix_len + 7 + 1;
	char unencoutputfilepath[unencoutputfilepathlen];
	bzero(unencoutputfilepath, unencoutputfilepathlen);
	/* - 7 to get rid of the trailing temp digits ( <filename>.comp.enc ) */
	/* - encsuffix_len to get rid of the trailing .enc ( <filename>.comp ) */
	strncpy(unencoutputfilepath, temp1, strlen(temp1) - 7 - encsuffix_len);
	/* Add a temp field our compressed .comp file ( <filename>.comp-XXXXXX ) */
	strncat(&unencoutputfilepath[0], "-XXXXXX", 8);
	r = mkstemp(unencoutputfilepath);
	close(r);
	unlink(unencoutputfilepath);

	dec_file(unencinputfilepath, unencoutputfilepath, key);

	if (KEEP_TEMP_ENC_FILES != 1) {
		if (0 != remove(temp1)) {
			fprintf(stderr, "WARNING: could not remove temporary encrypted .enc file!\n");
		}
	}
	
	/* unencoutputfilepath =  ( <filename>.comp-XXXXXX ) */
	int uncompoutputfilepathlen = strlen(unencoutputfilepath) + 1;
	char uncompoutputfilepath[uncompoutputfilepathlen];
	bzero(uncompoutputfilepath, uncompoutputfilepathlen);
	/* - 7 to get rid of the trailing temp digits ( <filename>.comp ) */
	/* - compsuffix_len to get rid of the trailing .comp ( <filename> )*/
	strncpy(uncompoutputfilepath, unencoutputfilepath, strlen(unencoutputfilepath) - 7 - compsuffix_len);

	if (0 != uncomp_file(unencoutputfilepath, uncompoutputfilepath)) {
		fprintf(stderr, "ERROR: could not uncompress file!\n");
	}
	if (KEEP_TEMP_COMP_FILES != 1) {
		if (0 != remove(unencoutputfilepath)) {
			fprintf(stderr, "WARNING: could not remove temporary compressed .comp file!\n");
		}
	}
	/* CSCD58 end of addition */

	return 1;
}

int main(int argc, char **argv){

	int	listenfd, connfd, port;
	struct sockaddr_in	servaddr;
	pid_t pid;

	if(argc != 2){
		printf("Invalid Number of Arguments...\n");
		printf("Usage: ./ftpserver <listen-port>\n");
		exit(-1);
	}

	sscanf(argv[1], "%d", &port);

	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(port);

	bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

	listen(listenfd, LISTENQ);

	while(1){
		connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
		printf("New Client Detected...\n");
		//child process---------------------------------------------------------------
		if((pid = fork()) == 0){
			close(listenfd);

			int datafds[NDATAFD], code, x = 0, client_port = 0;
			char recvline[MAXLINE+1];
			char client_ip[50], command[4096];

			while(1){
				bzero(recvline, (int)sizeof(recvline));
				bzero(command, (int)sizeof(command));

				//get client's data connection port
    			if((x = read(connfd, recvline, MAXLINE)) < 0){
    				break;
    			}
    			printf("*****************\n%s \n", recvline);
                if(strcmp(recvline, "QUIT") == 0){
                    printf("Quitting...\n");
                    char goodbye[1024];
                    sprintf(goodbye,"221 Goodbye");
                    write(connfd, goodbye, strlen(goodbye));
                    close(connfd);
                    break;
                }
    			get_client_ip_port(recvline, client_ip, &client_port);

    			int i;
                while (i < NDATAFD) {
                    if((setup_data_connection(datafds + i, client_ip, client_port, port)) < 0){
                            break;
                    }
                    i++;
                }

    			if((x = read(connfd, command, MAXLINE)) < 0){
    				break;
    			}

    			printf("-----------------\n%s \n", command);

    			code = get_command(command);
    			if(code == 1){
    				do_list(connfd, datafds[0], command);
    			}else if(code == 2){
    				do_retr(connfd, datafds, command);
    			}else if(code == 3){
    				do_stor(connfd, datafds[0], command);
    			}else if(code == 4){
                    char reply[1024];
                    sprintf(reply, "550 Filename Does Not Exist");
                    write(connfd, reply, strlen(reply));
                }
                i = 0;
    			close_data_connections(datafds);
			}
    		printf("Exiting Child Process...\n");
    		close(connfd);
    		_exit(1);
		}
		//end child process-------------------------------------------------------------
		close(connfd);
	}
}
