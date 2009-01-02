/* $Id$ */

#include <sys/types.h>
#include <sys/stat.h>

#include <elf_abi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtab.h"

static int		 elf_open(struct symtab *);
static void		 elf_close(struct symtab *);
static int		 elf_probe(struct symtab *);
static const char	*elf_getsymname(struct symtab *, unsigned long);
static unsigned long	 elf_getsymaddr(struct symtab *, const char *);

static struct binsw {
	int  (*open)(struct symtab *);
	void (*close)(struct symtab *);
	int  (*probe)(struct symtab *);
	const char *(*getsymname)(struct symtab *, unsigned long);
	unsigned long (*getsymaddr)(struct symtab *, const char *);
} binsw[] = {
	{ elf_probe, elf_close, elf_open, elf_getsymname, elf_getsymaddr }
};
#define NBINSW (sizeof(binsw) / sizeof(binsw[0]))

struct symtab *
symtab_open(char *fil)
{
	struct symtab *st;
	size_t i;

	if ((st = calloc(1, sizeof(*st))) == NULL)
		return (NULL);
	if ((st->st_fp = fopen(fil, "r")) == NULL)
		goto notbin;
	if (fstat(fileno(st->st_fp), &st->st_st) == -1)
		goto notbin;
	/* XXX: sloppy */
	if (fread(&st->st_hdr, 1, sizeof(st->st_hdr), st->st_fp) !=
	    sizeof(st->st_hdr))
		goto notbin;
	for (i = 0; i < NBINSW; i++)
		if (binsw[i].probe(st))
			break;
	if (i == NBINSW)
		goto notbin;
	if (!binsw[i].open(st))
		goto notbin;
	st->st_type = i;
	return (st);

notbin:
	symtab_close(st);
	return (NULL);
}

static int
elf_probe(struct symtab *st)
{
	if (st->st_ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
	    st->st_ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    st->st_ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
	    st->st_ehdr.e_ident[EI_MAG3] != ELFMAG3)
		return (0);
	if (st->st_ehdr.e_ident[EI_CLASS] != ELFCLASS32 &&
	    st->st_ehdr.e_ident[EI_CLASS] != ELFCLASS64)
		return (0);
	if (st->st_ehdr.e_ident[EI_DATA] != ELFDATA2LSB &&
	    st->st_ehdr.e_ident[EI_DATA] != ELFDATA2MSB)
		return (0);
	if (st->st_ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		return (0);
	if (st->st_ehdr.e_type != ET_EXEC)
		return (0);
	if (st->st_ehdr.e_shoff == 0L)
		return (0);
	return (1);
}

static int
elf_open(struct symtab *st)
{
	Elf_Shdr *strhdr;
	size_t siz;
	int i;

	if ((st->st_eshdrs = malloc(st->st_ehdr.e_shentsize *
	     st->st_ehdr.e_shnum)) == NULL)
		return (0);
	if (fseek(st->st_fp, st->st_ehdr.e_shoff, SEEK_SET) == -1)
		return (0);
	if (fread(st->st_eshdrs, st->st_ehdr.e_shentsize,
	    st->st_ehdr.e_shnum, st->st_fp) != st->st_ehdr.e_shnum)
		return (0);

	/* Get the names of the sections. */
	strhdr = &st->st_eshdrs[st->st_ehdr.e_shstrndx];
#if 0
	/* XXX: check for spoofed e_shstrndx. */
	if (st->st_ehdr.e_shstrndx > st->st_ehdr.e_shnum)
		/* XXX: kill st obj, because it is invalid. */
		return (0);
#endif
	siz = strhdr->sh_size;
	if ((st->st_eshnams = malloc(siz)) == NULL)
		return (0);
	if (fseek(st->st_fp, strhdr->sh_offset, SEEK_SET) == -1)
		return (0);
	if (fread(st->st_eshnams, 1, siz, st->st_fp) != siz)
		return (0);

	/* Find the string table section. */
	for (i = 0; i < st->st_ehdr.e_shnum; i++)
		if (strcmp(st->st_eshnams + st->st_eshdrs[i].sh_name,
		    ELF_STRTAB) == 0)
			break;
	if (i == st->st_ehdr.e_shnum)
		return (0);
	if ((st->st_esymnams = malloc(st->st_eshdrs[i].sh_size)) == NULL)
		return (0);
	if (fseek(st->st_fp, st->st_eshdrs[i].sh_offset, SEEK_SET) == -1)
		return (0);
	if (fread(st->st_esymnams, 1, st->st_eshdrs[i].sh_size,
	    st->st_fp) != st->st_eshdrs[i].sh_size)
		return (0);

	/* Find the symbol table section. */
	for (i = 0; i < st->st_ehdr.e_shnum; i++)
		if (strcmp(st->st_eshnams + st->st_eshdrs[i].sh_name,
		    ELF_SYMTAB) == 0)
			break;
	if (i == st->st_ehdr.e_shnum)
		return (0);
	st->st_ensyms = st->st_eshdrs[i].sh_size / sizeof(Elf_Sym);
	st->st_estpos = st->st_eshdrs[i].sh_offset;
	return (1);
}

void
symtab_close(struct symtab *st)
{
	binsw[st->st_type].close(st);
	if (st->st_fp != NULL)
		(void)fclose(st->st_fp);
	free(st);
}

static void
elf_close(struct symtab *st)
{
	free(st->st_esymnams);
	free(st->st_eshnams);
	free(st->st_eshdrs);
}

const char *
symtab_getsymname(struct symtab *st, unsigned long addr)
{
	return (binsw[st->st_type].getsymname(st, addr));
}

unsigned long
symtab_getsymaddr(struct symtab *st, const char *name)
{
	return (binsw[st->st_type].getsymaddr(st, name));
}

static const char *
elf_getsymname(struct symtab *st, unsigned long addr)
{
	Elf_Sym esym;
	int i;

	/* Find desired symbol in table. */
	if (fseek(st->st_fp, st->st_estpos, SEEK_SET) == -1)
		return (NULL);
	for (i = 0; i < st->st_ensyms; i++) {
		if (fread(&esym, 1, sizeof(esym), st->st_fp) !=
		    sizeof(esym))
			return (NULL);
		if (ELF_ST_TYPE(esym.st_info) == STT_FUNC) {
#if 0
			/* Check for spoofing. */
			if ((long long)esym.st_name > st->st_st.st_size)
				continue;
#endif
			/* XXX: check for st_name < sh_size */
			if (esym.st_value == addr)
				return (&st->st_esymnams[esym.st_name]);
		}
	}
	return (NULL);
}

static unsigned long
elf_getsymaddr(struct symtab *st, const char *name)
{
	Elf_Sym esym;
	int i;

	if (fseek(st->st_fp, st->st_estpos, SEEK_SET) == -1)
		return (NULL);
	for (i = 0; i < st->st_ensyms; i++) {
		if (fread(&esym, 1, sizeof(esym), st->st_fp) !=
		    sizeof(esym))
			return (NULL);
		if (ELF_ST_TYPE(esym.st_info) == STT_FUNC) {
#if 0
			/* Check for spoofing. */
			if ((long long)esym.st_name > st->st_st.st_size)
				continue;
#endif
			/* XXX: check for st_name < sh_size */
			if (strcmp(name, &st->st_esymnams[esym.st_name]) == 0)
				return (esym.st_value);
		}
	}
	return (NULL);
}
