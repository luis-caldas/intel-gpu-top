#include <stdint.h>

#define MAX_INSTDONE_BITS            100

struct instdone_bit {
	uint32_t reg;
	uint32_t bit;
	const char *name;
};

extern struct instdone_bit instdone_bits[MAX_INSTDONE_BITS];
extern int num_instdone_bits;

void init_instdone_definitions(uint32_t devid);
