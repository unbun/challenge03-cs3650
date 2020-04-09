#include <stdio.h>
#include <stdint.h>
#include "bitmap.h"

// ATTRIBUTION:
// bitmap arithmetic: Ryan Layer
// https://layerlab.org/2018/06/07/My-bitmap.html

int 
bitmap_get(void* bm, int ii) {
    return ((uint8_t *) bm)[ii / 8] & (1 << (ii % 8));
}

void
bitmap_put(void* bm, int ii, int vv) {

    int idx = ii / 8; // 32 ???
    int shift = ii % 8; // 32 ???

    uint8_t* ubm = (uint8_t*)bm;

  	if (vv == 0) {
        ubm[idx] &= ~(1 << (shift));
  	} else {
        ubm[idx] |= 1 << (shift);
  	}
}

void 
bitmap_print(void* bm, int size) {
	printf("========== bitmap [%d] ==========\n", size);

    int bytes = size / 8;
    if (size % 8)
    {
        bytes = (size / 8) + 1;
    }

	for (int ii = 0; ii < bytes; ii++) {
		int element = bitmap_get(bm, ii);
		printf("[%d] = %d\n", ii, element);
	}

	printf("=================================\n");
}	
