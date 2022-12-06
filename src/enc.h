#ifndef ENC_HEADER
#define ENC_HEADER

#define 	MAXLINEENC 4096

void init_enc();
uint64_t sq_mp(uint64_t, uint64_t, uint64_t);
int enc_file(char[MAXLINEENC+1], char[MAXLINEENC+1], uint32_t[4]);
int dec_file(char[MAXLINEENC+1], char[MAXLINEENC+1], uint32_t[4]);

#endif
