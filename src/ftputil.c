#include <arpa/inet.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#if DEBUG_LEVEL >= 1
#include <time.h>
#endif

#include "comp.h"
#include "enc.h"
#include "ftputil.h"
#include "pec-ftp.h"


#if DEBUG_LEVEL >= 1
void timespecsubtract(struct timespec *a, struct timespec *b, struct timespec *c) {
	c->tv_sec = a->tv_sec - b->tv_sec;
	c->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (c->tv_nsec < 0) {
		c->tv_nsec = 1000000000 + c->tv_nsec;
		c->tv_sec -= 1;
 	}
}


/* '*s' should point to at least (20 + 1 + num_dec + 1 = 31) free bytes */
void timespecstr(struct timespec *t, char *s, unsigned int num_dec) {
	if (num_dec >= 1) {
		/* tv_nsec has a max value of 999,999,999, so we allocate for printing that
		* value in base 10 + the null term */
		char decimals[10];
		sprintf(decimals, "%09ld", t->tv_nsec);
		decimals[num_dec] = '\0';
		sprintf(s, "%ld.%s", t->tv_sec, decimals);
	} else {
		sprintf(s, "%ld", t->tv_sec);
	}
}
#endif


/** Takes a string and modifies it such that there is no leading or trailing
 * whitespace.
 *
 * \param '*str' the input string which will be modified.
 * \return void.
 */
void trim(char *str) {
	int i;
	int len = strlen(str);
	int begin = 0;
	int end = len - 1;

	/* Find index of first non-whitespace character */
	while ((begin < len) && isspace((unsigned char) str[begin]))
		begin++;

	/* Find index of last non-whitespace character */
	while ((end >= begin) && isspace((unsigned char) str[end]))
		end--;

	/* Shift all characters caught between leading and trailing whitespace
	 * to the start of the string */
	for (i = begin; i <= end; i++)
		str[i - begin] = str[i];

	/* Note: if the string contained nothing but whitespace, a string of 0
	 * length is returned */
	str[i - begin] = '\0';
}


/* Modifies 'port' to contain the port of the socket represented by 'fd'.
 *
 * \param 'fd' a file descriptor representing a socket.
 * \param '*port' a pointer to a short which will have its value modified to
 *     the port of the socket.
 * \return 0 upon success, a negative int upon failure.
 */
int get_port(int fd, uint16_t *port){
	struct sockaddr_in s;
	socklen_t len = sizeof(s);

	if (getsockname(fd, (struct sockaddr *)&s, &len) == -1) {
		return -1;
	} else {
		(*port) = ntohs(s.sin_port);
		return 0;
	}
}


// TODO: what does this function do? My best guess at the moment is that it
// modifies 'ip', and 'port' to contain the IP address and port of the socket
// represented by 'fd'
int get_ip_port(int fd, char *ip, uint16_t *port){
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);

	getsockname(fd, (struct sockaddr *) &addr, &len);
	// TODO: potential security risk. Switch to strncpy?
	sprintf(ip, inet_ntoa(addr.sin_addr));
	*port = (uint16_t) ntohs(addr.sin_port);
	return 1;
}


/** Takes a string 'filename' representing a file path and returns a string
 * that is the given file path + the compression extension, the encryption
 * extension and a series of temp file digits. The caller of this function must
 * free the returned string.
 *
 * \param '*filename' a string representing a filepath where the decrypted and
 *     decompressed file will be written.
 * \return a temp file for receiving a file upon success, a NULL pointer on
 *     failure.
 */
char * temp_recv_name(char * filename) {
	char * c_out_fp_pure;
	char * recv_fp;
	/* Get the "pure" name for the compressed version of the file, i.e., the
	 * temp compression file name minus the 'mkstemp()' digits (
	 * '<filename>.comp' ) */
	if ( (c_out_fp_pure = compression_name(filename)) == NULL) {
		fprintf(stderr, "ERROR: failed to receive file!\n");
		return NULL;
	}

	/* Take the "pure" compressed name and get a temp encryption name for it
	 * ( '<filename>.comp.enc-XXXXXX' ) */
	if ( (recv_fp = temp_encryption_name(c_out_fp_pure)) == NULL) {
		fprintf(stderr, "ERROR: failed to receive file!\n");
		return NULL;
	}

	free(c_out_fp_pure);

	return recv_fp;
}


/** Takes a string 'filename' representing an input file path, a key used to
 * encrypt the file, and pointer to a pointer serving as a return variable, and
 * encrypts and compresses the file, storing the result at
 * '*(ret_prepared_fp)'. The caller of this function must free
 * '*(ret_prepared_fp)'.
 *
 * \param '*filename' a string representing a filepath to the file to be
 *     compressed and encrypted.
 * \param 'key' a key used for the decryption part of this process.
 * \param '**ret_prepared_fp' a pointer which will be modified to contain
 *     a pointer to the filename of the temporary prepared file generated by
 *     this function.
 * \return 0 upon success, a negative int upon failure.
 */
int prepare_file(char * filename, uint32_t key[4], char ** ret_prepared_fp) {
	char * c_out_fp;
	char * c_out_fp_pure;
	char * e_out_fp;

	/* Get temp name for compressed version of the file */
	if ( (c_out_fp = temp_compression_name(filename)) == NULL) {
		fprintf(stderr, "ERROR: could not compress file!\n");
		return -1;
	}

#if DEBUG_LEVEL >= 1
	struct timespec ts_start;
	struct timespec ts_end;
	struct timespec ts_elapsed;
	char timestring[31];

	clock_gettime(CLOCK_MONOTONIC, &ts_start);
#endif
	/* Compress content of file at 'filename' writing output to file at
	 * 'c_out_fp' */
	if (0 != comp_file(filename, c_out_fp)) {
		fprintf(stderr, "ERROR: could not compress file!\n");
		return -1;
	}
#if DEBUG_LEVEL >= 1
	clock_gettime(CLOCK_MONOTONIC, &ts_end);
	timespecsubtract(&ts_end, &ts_start, &ts_elapsed);
	timespecstr(&ts_elapsed, &timestring[0], 2);
	fprintf(stderr, "(%d) STATUS: preparing file: compression took %s seconds\n", \
		getpid(), &timestring[0]);
#endif

	/* Get the "pure" name for the compressed version of the file, i.e.,
	 * the temp compression file name minus the 'mkstemp()' digits */
	if ( (c_out_fp_pure = compression_name(filename)) == NULL) {
		fprintf(stderr, "ERROR: could not encrypt file!\n");
		return -1;
	}
	/* CAE3b: Get temp name for encrypted version of the compressed file */
	if ( (e_out_fp = temp_encryption_name(c_out_fp_pure)) == NULL) {
		fprintf(stderr, "ERROR: could not encrypt file!\n");
		return -1;
	}

	free(c_out_fp_pure);

#if DEBUG_LEVEL >= 1
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
#endif
	/* Encrypt content of file at 'filename' writing output to file at
	 * 'e_out_fp' */
	if (0 != enc_file(c_out_fp, e_out_fp, key)) {
		fprintf(stderr, "ERROR: could not encrypt file!\n");
		return -1;
	}
#if DEBUG_LEVEL >= 1
	clock_gettime(CLOCK_MONOTONIC, &ts_end);
	timespecsubtract(&ts_end, &ts_start, &ts_elapsed);
	timespecstr(&ts_elapsed, &timestring[0], 2);
	fprintf(stderr, "(%d) STATUS: preparing file: encryption took %s seconds\n", \
		getpid(), &timestring[0]);
#endif

	if (KEEP_TEMP_COMP_FILES != 1) {
		if (0 != remove(c_out_fp)) {
			fprintf(stderr, "WARNING: could not remove temporary compressed .comp file!\n");
		} 
	}

	free(c_out_fp);

	(*ret_prepared_fp) = e_out_fp;

	return 0;
}


/** Takes a string 'filename' representing an output file path, a string
 * representing a path to a received file 'recv_fp', and a key used to decrypt
 * the file and decrypts and decompresses the file, storing the result at
 * 'filename'.
 *
 * \param '*filename' a string representing a filepath where the decrypted and
 *     decompressed file will be written.
 * \param '*recv_fp' a string representing a filepath to a received file which
 *     will be compressed and encrypted.
 * \param 'key' a key used for the decryption part of this process.
 * \return 0 upon success, a negative int upon failure.
 */
int process_received_file(char * filename, char * recv_fp, uint32_t key[4]) {
	char * dec_out_fp;
	/* Make temporary name for decryted (but not yet decompressed) file
	 * ( '<filename>.comp-XXXXXX' ) */
	if ( (dec_out_fp = temp_compression_name(filename)) == NULL) {
		fprintf(stderr, "ERROR: could not decrypt file!\n");
		return -1;
	}

#if DEBUG_LEVEL >= 1
	struct timespec ts_start;
	struct timespec ts_end;
	struct timespec ts_elapsed;
	char timestring[31];

	clock_gettime(CLOCK_MONOTONIC, &ts_start);
#endif
	/* Decrypt content of file at 'recv_fp' writing output to file at
	 * 'dec_out_fp' */
	if (0 != dec_file(recv_fp, dec_out_fp, key)) {
		fprintf(stderr, "ERROR: could not decrypt file!\n");
		return -1;
	}
#if DEBUG_LEVEL >= 1
	clock_gettime(CLOCK_MONOTONIC, &ts_end);
	timespecsubtract(&ts_end, &ts_start, &ts_elapsed);
	timespecstr(&ts_elapsed, &timestring[0], 2);
	fprintf(stderr, "(%d) STATUS: preparing file: decryption took %s seconds\n", \
		getpid(), &timestring[0]);
#endif

	if (KEEP_TEMP_ENC_FILES != 1) {
		if (0 != remove(recv_fp)) {
			fprintf(stderr, "WARNING: could not remove temporary encrypted .enc file!\n");
		}
	}

#if DEBUG_LEVEL >= 1
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
#endif
	/* Uncompress content of file at 'dec_out_fp' writing output to file at
	 * 'filename' */
	if (0 != uncomp_file(dec_out_fp, filename)) {
		fprintf(stderr, "ERROR: could not uncompress file!\n");
		return -1;
	}
#if DEBUG_LEVEL >= 1
	clock_gettime(CLOCK_MONOTONIC, &ts_end);
	timespecsubtract(&ts_end, &ts_start, &ts_elapsed);
	timespecstr(&ts_elapsed, &timestring[0], 2);
	fprintf(stderr, "(%d) STATUS: preparing file: uncompression took %s seconds\n", \
		getpid(), &timestring[0]);
#endif

	if (KEEP_TEMP_COMP_FILES != 1) {
		if (0 != remove(dec_out_fp)) {
			fprintf(stderr, "WARNING: could not remove temporary compressed .comp file!\n");
		}
	}

	free(dec_out_fp);

	return 0;
}


void close_data_connections(int *datafds) {
	for (int i = 0; i < NDATAFD; i++) {
		close(datafds[i]);
	}
}
