#include <ctype.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "LzmaLib.h"
#include "pec-ftp.h"

int read_bytes(unsigned char *ret, size_t num_bytes, FILE *f) {
	/* {{{ */
	size_t nmem_read = 0;
	if (num_bytes != (nmem_read = fread(ret, 1, num_bytes, f)) ) {
		fprintf(stderr, "ERROR: read_bytes(): could not read %lu bytes, was only able to read %lu\n", num_bytes, nmem_read);
		return -1;
	}

	return 0;
	/* }}} */
}

unsigned char * read_chunk_of_file(FILE * f, uint64_t * ret_len) {
	/* {{{ */
	unsigned char * ret = malloc(MAX_CHUNK);
	/* Alloc call failed, exit */
	if (ret == NULL) {
		fprintf(stderr, "ERROR: read_chunk_of_file(): could not allocate buffer for file\n");
		return NULL;
	}

	size_t nmem_read = fread(&ret[0], 1, MAX_CHUNK, f);

	*ret_len = nmem_read;

	return ret;
	/* }}} */
}

void clear_file(char* file) {
	/* {{{ */
	FILE* victim = fopen(file, "w");
	if (victim == NULL) {
		perror("fopen (clear_file)");
		return;
	}
	fprintf(victim, "");
	fclose(victim);

	return;
	/* }}} */
}

int comp_file(char * inputfilepath, char * outputfilepath) {
	/* {{{ */
	struct stat s;
	stat(inputfilepath, &s);

	/* If the size of the file is too large to be done in one pass */
	if (s.st_size > MAX_CHUNK) {
		fprintf(stderr, "WARNING: File is bigger than max chunk size and cannot be done in one pass. It must be compressed incrementally.\n");
	}

	FILE *in_file = fopen(inputfilepath, "rb");
	if (in_file == NULL) {
		perror("fopen (main)");
		return -1;
	}

	clear_file(outputfilepath);

	FILE *out_file = fopen(outputfilepath, "ab");
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

	return 0;
	/* }}} */
}

int uncomp_file(char * inputfilepath, char * outputfilepath) {
	/* {{{ */
	struct stat s;
	stat(inputfilepath, &s);

	FILE * in_file = fopen(inputfilepath, "rb");
	if (in_file == NULL) {
		perror("fopen (main)");
		return -1;
	}

	clear_file(outputfilepath);

	FILE * out_file = fopen(outputfilepath, "ab");
	if (out_file == NULL) {
		perror("fopen (main)");
		return -1;
	}

	off_t bytes_left = s.st_size;
	unsigned char *inbuf = NULL;

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
	/* }}} */
}
