#include <stdio.h>
#include "extern.h"

int main()
{
	ud_t obj;
	unsigned char code[] = {0x55, 0x48, 0x89, 0xE5, 0x41, 0x56};
	
	ud_init(&obj);
	ud_set_mode(&obj, 64);
	ud_set_input_buffer(&obj, code, sizeof(code));
	int count = 0;

	while (ud_disassemble(&obj))
	{
		printf("Decoded instruction...\n");
		count += ud_insn_len(&obj);
	}

	printf("Count: %d\n", count);
	return 0;
}
