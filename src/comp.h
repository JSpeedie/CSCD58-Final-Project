#include <stdint.h>
#include <stdio.h>

/* Change MAX_CHUNK value as needed depending on RAM constraints of the systems.
 * Max value is 4294967295 */
#define MAX_CHUNK 2147483647
#define PEC_HEADER_SIZE 9
#define CHUNK_HEADER_SIZE 8

int read_bytes(unsigned char *ret, size_t num_bytes, FILE *f);

unsigned char * read_chunk_of_file(FILE * f, uint64_t * ret_len);

void clear_file(char* file);

int comp_file(char * inputfilepath, char *outputfilepath);

int uncomp_file(char * inputfilepath, char * outputfilepath);
