#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>

#ifdef MPI
#include <mpi.h>
MPI_Group world;
#endif

#include "psc_util/crc.h"

#ifndef MIN
# define MIN(a,b) (((a)<(b)) ? (a): (b))
#endif
#ifndef MAX
# define MAX(a,b) (((a)>(b)) ? (a): (b))
#endif

int pes, mype;
unsigned long crc=0, debug=0;
ssize_t bufsz=131072;

psc_crc_t filecrc;

char file[PATH_MAX];
char *buf;

static void 
sft_help(void) { 
	fprintf(stderr, "sft [-n] [-d] [-f filename] [-c]\n"
		"\t -d (enable debugging output)\n"
		"\t -c (enable MD5 checksummming)\n"
		"\t -f filename (specify the filename to read)\n");
	exit(1);
}

static void 
sft_getopt(int argc,  char *argv[]) { 
#define ARGS "dhcn:f:"
	int have_file=0;
	char c;
	optarg = NULL; 
	
	while ((c = getopt(argc, argv, ARGS)) != -1) {
		switch (c) {  
			
		case 'h':
			sft_help();
			break;
		case 'c':
			crc = 1;
			PSC_CRC_INIT(filecrc);
			break;
		case 'd':
			debug = strtol(optarg, NULL, 10);
			break;			
		case 'f':			
			strncpy(file, optarg, PATH_MAX);
			have_file = 1;
			break;
		case 'b':
			bufsz = strtol(optarg, NULL, 10);
			break;

		default : 			
			fprintf(stderr, "Invalid option '%s'\n", optarg);
			sft_help();
		}
	}

	if (!have_file) {
		fprintf(stderr, "No input file specified");
		sft_help();
	}
} 

static void 
sft_barrier(void)
{
#ifdef MPI
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

static void 
sft_parallel_init(void)
{
#ifdef MPI	
	if (MPI_Init(&argc, &argv) != MPI_SUCCESS)
		abort();
	
	MPI_Comm_size(MPI_COMM_WORLD, &pes);
	MPI_Comm_rank(MPI_COMM_WORLD, &mype);
	
	rc = MPI_Comm_group(MPI_COMM_WORLD, &world);
	if (rc != MPI_SUCCESS)
		abort();
#endif
	return;
}

static void
sft_parallel_finalize(void)
{
#ifdef MPI
	MPI_Finalize();
#endif
	return;
}

int 
main(int argc, char *argv[]) {
	int rc, fd;
	struct stat stb;
	ssize_t rem, szrc;
	size_t tmp;
	
	sft_getopt(argc, argv);

	buf = malloc(bufsz);
	if (!buf)
		abort();
	
	sft_parallel_init();
	sft_barrier();

	fd = open(file, O_RDONLY);
	if (fd < 0)
		abort();

	if (fstat(fd, &stb))
		abort();

	if (debug)
		fprintf(stdout, "filesize=%lu", stb.st_size);
	
	rem = stb.st_size;

	while (rem) {		
		tmp = MIN(rem, bufsz);
		szrc = read(fd, buf, tmp);
		if (szrc != (ssize_t)tmp) {
			perror("failed to read");
			abort();
		} else {
			rem -= tmp;
			if (crc)
				PSC_CRC_ADD(filecrc, buf, tmp);
		}
	}

	if (crc) {
		PSC_CRC_FIN(filecrc);
		fprintf(stdout, "F '%s' CRC=0x%lx\n", file, filecrc);
	}	
	close(fd);

	sft_parallel_finalize();
	return (0);
}
