typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

int main(int argument_count, char** arguments)
{
	(void)arguments;
	volatile uint8_t* page = (void*)0x0000000fffffc000ULL;
	volatile uint8_t* read_only_page = (void*)0x0000000fffff4000ULL;
	if (argument_count > 1) {
		page[0] = 0;
		return 20;
	}
	if (*(volatile uint16_t*)(page + 0x01e) != 3)
		return 1;
	uint64_t capabilities = *(volatile uint64_t*)(page + 0x010);
	if (*(volatile uint32_t*)(page + 0x020) != (uint32_t)capabilities)
		return 2;
	if (page[0x022] != 3 || page[0x034] != 3 ||
	    page[0x035] != 3 || page[0x036] != 3)
		return 3;
	if ((capabilities & 0x00ff0000ULL) != 0x00030000ULL)
		return 4;
	if (!(capabilities & 0x00000100ULL))
		return 5;
	if (page[0x025] < 12 || page[0x025] > 16 ||
	    page[0x037] != page[0x025])
		return 6;
	if (read_only_page[0x025] != page[0x025] ||
	    read_only_page[0x037] != page[0x037])
		return 7;
	if (!*(volatile uint64_t*)(page + 0x038))
		return 8;
	return 0;
}
