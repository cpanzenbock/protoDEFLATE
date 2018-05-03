#include <stdio.h>
#include <stdlib.h>

/*
	Absolute minimum implementation of DEFLATE (and even then with shortcuts).

	Simple DEFLATE spec:
		- Treats whole file as one block.
		- Do the entire LZ77, putting metadata of number of backreferences and
			each of their byte addresses
		- With a frequency analysis, generate huffman tree
		- Based on huffman tree to huffman encoding, putting metadata of huffman tree

	Simplified LZ77 spec:
		- For each byte in the file
		-- Go through the previous 64K bytes
		-- For each byte match in the window
		--- Figure out the longest match length
		-- Replace the current byte and subsequent bytes with a backreference [D:L] 3B
		-- Increment number of backreferences and queue the cursor's address

	Simplified Huffman spec:
		- Count up frequencies of all characters
		- Generate huffman tree...
		- Generate bitstream using the dictionary
		We'll not do this for now though.

*/

//#define DEBUG_LV0 1
//#define DEBUG_LV1 1
//#define DEBUG_LV2 1

#define FILE_MAX_INPUT_SIZE 16777216	// 2^24
#define LZ_MAX_BACK_REFS 16777216		// 2^24
#define LZ_WINDOW_SIZE 65536 			// 2^16
#define LZ_MAX_MATCH_LEN 256 			// 2^8

typedef struct _backref {
	uint8_t found;
	uint8_t disp_hi;
	uint8_t disp_lo;
	uint8_t length;
} bref;

uint32_t uncompressed_size;
uint64_t lzcompressed_size;
uint32_t num_backrefs = 0;
bref backrefs[LZ_MAX_BACK_REFS];
uint32_t backref_locs[LZ_MAX_BACK_REFS];

uint8_t uncompressed[FILE_MAX_INPUT_SIZE] = {0};
uint8_t lzcompressed[3*FILE_MAX_INPUT_SIZE] = {0};

void extract_file(char *filename);
void save_compression(char *filename);
void lz77();
bref get_longest_match(uint32_t u_cursor);
uint8_t get_w1(uint32_t in);
uint8_t get_w0(uint32_t in);
void huffman();

int main (int argc, char *argv[]) {

	#ifdef DEBUG_LV0
	printf("Entering main function...\n");
	#endif

	extract_file(argv[1]);
	lz77();
	huffman();
	save_compression(argv[2]);
	return EXIT_SUCCESS;
}

void extract_file(char *filename) {

	#ifdef DEBUG_LV0
	printf("Extracting file...\n");
	#endif

	FILE *f = fopen(filename, "r");
	uncompressed_size = fread(uncompressed, sizeof(uint8_t), FILE_MAX_INPUT_SIZE, f);
	fclose(f);

	#ifdef DEBUG_LV0
	printf("Extracting file complete...\n");
	#endif
}

void save_compression(char *filename) {
	#ifdef DEBUG_LV0
	printf("Saving compression to file [num_backrefs = %d]\n", num_backrefs);
	#endif

	FILE *f = fopen(filename, "w");
	fwrite(&lzcompressed_size, sizeof(uint64_t), 1, f);
	fwrite(&num_backrefs, sizeof(uint64_t), 1, f);
	fwrite(backref_locs, sizeof(uint32_t), num_backrefs, f);
	fwrite(lzcompressed, sizeof(uint8_t), lzcompressed_size, f);
	fclose(f);

	#ifdef DEBUG_LV0
	printf("Saving compression to file compelete...\n");
	#endif
}

void lz77() {
	#ifdef DEBUG_LV0
	printf("LZ77 commencing...\n");
	#endif

	uint32_t u_cursor = 0; //uncompressed index
	uint64_t l_cursor = 0; //lzcompressed index
	bref ref;
	while (u_cursor < uncompressed_size) {
		ref = get_longest_match(u_cursor);
		if (ref.found && num_backrefs < LZ_MAX_BACK_REFS) {
			lzcompressed[l_cursor] = ref.disp_hi;
			lzcompressed[l_cursor+1] = ref.disp_lo;
			lzcompressed[l_cursor+2] = ref.length;
			backref_locs[num_backrefs] = u_cursor;
			backrefs[num_backrefs] = ref;
			num_backrefs++;
			u_cursor += ref.length;
			l_cursor += 3;
		} else {
			lzcompressed[l_cursor] = uncompressed[u_cursor];
			u_cursor++;
			l_cursor++;
		}
	}
	lzcompressed_size = l_cursor;

	#ifdef DEBUG_LV0
	printf("LZ77 complete...\n");
	#endif
}

bref get_longest_match(uint32_t u_cursor) {
	#ifdef DEBUG_LV0
	printf("Finding longest match at cursor/char: %d|%c...\n", u_cursor, uncompressed[u_cursor]);
	#endif

	uint32_t w_cursor; //window index
	uint32_t m_cursor; //match index

	bref ref;
	ref.found = 0;
	ref.length = 7; // Match needs to be greater than 3 to be worth it.

	int match_len;
	uint32_t displacement;

	w_cursor = u_cursor - LZ_WINDOW_SIZE;
	if (u_cursor < LZ_WINDOW_SIZE) w_cursor = 0;
	while (w_cursor < u_cursor) {
		if (uncompressed[u_cursor] == uncompressed[w_cursor]) {

			#ifdef DEBUG_LV1
			printf("\tMatched initial '%c' with '%c' @ w_cursor=%d\n", uncompressed[u_cursor], uncompressed[w_cursor], w_cursor);
			#endif

			match_len = 1;
			m_cursor = w_cursor + 1;
			while (m_cursor < u_cursor && uncompressed[u_cursor+match_len] == uncompressed[m_cursor] && match_len < LZ_MAX_MATCH_LEN) {

				#ifdef DEBUG_LV2
				printf("\t\tMatched next '%c' with '%c' @ m_cursor=%d\n", uncompressed[u_cursor+match_len], uncompressed[m_cursor], m_cursor);
				#endif

				match_len++;
				m_cursor++;
			}
			if (match_len > ref.length) {
				ref.found = 1;
				ref.length = match_len;
				displacement = u_cursor - w_cursor;
				ref.disp_hi = get_w1(displacement);
				ref.disp_lo = get_w0(displacement);
			}
			w_cursor += match_len;
		} else {
			w_cursor++;
		}
	}

	#ifdef DEBUG_LV0
	printf("Found longest match with [status, address hi:lo, displacement, length]: %d, %x:%x, %d, %d...\n", ref.found, ref.disp_hi, ref.disp_lo, displacement, ref.length);
	#endif

	return ref;
}

uint8_t get_w0(uint32_t in) { // MSB w3|w2|w1|w0 LSB
	#ifdef DEBUG_LV0
	printf("Filtering higher byte...\n");
	#endif

	return (uint8_t)(in & 0x000000FF);
}

uint8_t get_w1(uint32_t in) { // MSB w3|w2|w1|w0 LSB
	#ifdef DEBUG_LV0
	printf("Filtering lower byte...\n");
	#endif

	return (uint8_t)((in & 0x0000FF00) >> 8);
}

void huffman() {
	// makes no changes for now
	// not a priority to implement seeing as it no longer works with bytes
	// which complicates things
}
