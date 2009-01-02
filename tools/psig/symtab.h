/* $Id$ */

#include <sys/types.h>
#include <sys/stat.h>

#include <elf.h>

struct symtab {
	FILE			*st_fp;
	struct stat		 st_st;
	int			 st_type;
	char			*st_eshnams;
	char			*st_esymnams;
	int			 st_ensyms;
	off_t			 st_estpos;
	union symtab_hdr {
		Elf32_Ehdr	 hdr_elf32;
		Elf64_Ehdr	 hdr_elf64;
	} st_hdr;
	union symtab_data {
		Elf32_Shdr	*elf_shdrs32;
		Elf64_Shdr	*elf_shdrs64;
	} st_data;
#define st_ehdr32	st_hdr.hdr_elf32
#define st_ehdr64	st_hdr.hdr_elf64
#define st_eshdrs32	st_data.elf_shdrs32
#define st_eshdrs64	st_data.elf_shdrs64
};

struct symtab	*symtab_open(char *);
const char	*symtab_getsymname(struct symtab *, unsigned long);
unsigned long	 symtab_getsymaddr(struct symtab *, const char *);
void		 symtab_close(struct symtab *);
