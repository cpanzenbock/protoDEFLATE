#include <stdio.h>
#include <stdlib.h>

/* Reverse process of deflate */

//#define DEBUG_LV0 1
//#define DEBUG_LV1 1
//#define DEBUG_LV2 1

#define FILE_MAX_OUTPUT_SIZE 16777216	// 2^24
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
uint32_t backref_locs[LZ_MAX_BACK_REFS];

uint8_t uncompressed[FILE_MAX_OUTPUT_SIZE] = {0};
uint8_t lzcompressed[3*FILE_MAX_OUTPUT_SIZE] = {0};

void extract_file(char *filename);
void save_decompression(char *filename);
void inv_huffman();
void inv_lz77();

int main (int argc, char *argv[]) {

	#ifdef DEBUG_LV0
	printf("Entering main function...\n");
	#endif

	extract_file(argv[1]);
	inv_huffman();
	inv_lz77();
	save_decompression(argv[2]);
	return EXIT_SUCCESS;
}

void extract_file(char *filename) {

	#ifdef DEBUG_LV0
	printf("Extracting file...\n");
	#endif

	FILE *f = fopen(filename, "r");
	fread(&lzcompressed_size, sizeof(uint64_t), 1, f);
	fread(&num_backrefs, sizeof(uint64_t), 1, f);
	fread(backref_locs, sizeof(uint32_t), num_backrefs, f);
	lzcompressed_size = fread(lzcompressed, sizeof(uint8_t), (3*FILE_MAX_OUTPUT_SIZE), f);
	fclose(f);

	#ifdef DEBUG_LV0
	printf("Extracting file complete...\n");
	#endif
}

void save_decompression(char *filename) {
	#ifdef DEBUG_LV0
	printf("Saving decompression to file [num_backrefs = %d]\n", num_backrefs);
	#endif

	FILE *f = fopen(filename, "w");
	fwrite(uncompressed, sizeof(uint8_t), uncompressed_size, f);
	fclose(f);

	#ifdef DEBUG_LV0
	printf("Saving decompression to file compelete...\n");
	#endif
}

void inv_lz77() {
	uncompressed_size = 0;
	uint32_t u_cursor = 0; //uncompressed index
	uint64_t l_cursor = 0; //lzcompressed index
	uint32_t curr_backref_num = 0;
	bref curr_ref;
	int match_len;
	uint32_t displacement;
	while (l_cursor < lzcompressed_size) {
		if (curr_backref_num < num_backrefs) {
			if (u_cursor == backref_locs[curr_backref_num]) {
				// Hit a backreference
				curr_ref.disp_hi = lzcompressed[l_cursor];
				curr_ref.disp_lo = lzcompressed[l_cursor+1];
				curr_ref.length = lzcompressed[l_cursor+2];
				match_len = 0;
				displacement = (curr_ref.disp_hi << 8) | (curr_ref.disp_lo);
				// Copy curr_ref.length bytes from 'displacement' bytes back in uncompressed
				while(match_len < curr_ref.length) {
					uncompressed[u_cursor] = uncompressed[u_cursor-displacement];
					u_cursor++;
					match_len++;
				}
				l_cursor += 3;
				curr_backref_num++;
			} else { // No backreference, transcribe byte
				uncompressed[u_cursor] = lzcompressed[l_cursor];
				u_cursor++;
				l_cursor++;
			}
		} else { // No more backreferences, just transcribe the rest of doc directly
			uncompressed[u_cursor] = lzcompressed[l_cursor];
			u_cursor++;
			l_cursor++;
		}
	}
	uncompressed_size = u_cursor;
}

void inv_huffman() {
	// nothing
}
