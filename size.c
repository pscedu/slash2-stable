#include <stdio.h>
#include <sys/types.h>

int main() { 
	printf("SIZE_T %d\nOFF_T %d\nSSIZE_T %d\nLONG %d, INT %d\nLONG LONG %d", 
		sizeof(size_t), 
		sizeof(off_t), 
		sizeof(ssize_t), 
		sizeof(long), 
		sizeof(int),
		sizeof(long long));
	exit(0);
}
	
