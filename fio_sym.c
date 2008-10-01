#include "fio.h"
#include "fio_sym.h"
#include "fio_symtable.h"

struct symtable_t * get_symbol(const char *name) { 
  struct symtable_t *e = NULL;

  BDEBUG("symbol lookup '%s'\n", name);

  for (e = sym_table; e != NULL && e->name != NULL ; e++) { 
    if (e->name && !strcmp(e->name, name))       
      break;
  }  

  if (e == NULL || e->name == NULL) { 
    WARN("Symbol '%s' was not found\n", name);
  }
  return e;
}

struct symtable_t * func_lookup(unsigned long addr) { 
  struct symtable_t *e = NULL;

  for (e = sym_table; e != NULL && e->name != NULL ; e++) 
    if (e->sym_type && FIO_FUNCTION) 
      if ((unsigned long)e->io_func == addr) break;
  
  if (e == NULL || e->name == NULL) { 
    WARN("Symbol '%lx' was not found\n", addr);
  }
  return e;
}

char * func_addr_2_name(unsigned long addr) { 
  struct symtable_t *e = func_lookup(addr);
  
  ASSERT(e != NULL);
  return (e->name);
}


#define GETSYM(snam, sym) do {			       \
  sym = get_symbol(snam);                              \
  if (sym == NULL) {                                   \
    WARN("Invalid config entry %s\n", snam);           \
    ASSERT(0);                                         \
  }                                                    \
 } while(0);


void store_func(const char *func_name) { 
  extern GROUP_t      *currentGroup;
  struct symtable_t   *sym =  get_symbol(func_name);
  int num_iotests          =  currentGroup->num_iotests; 
  struct io_routine_t *ior = &currentGroup->iotests[num_iotests];

  ASSERT(sym != NULL && ior != NULL);
  
  BDEBUG("Test Recipe Comp: %s func_ptr %p\n", 
	func_name, sym->io_func);
  
  ior->io_routine[ior->num_routines] = strdup(func_name);
  ior->num_routines++;
  return;
}

void store_tok_val(const char *tok, char *val) {
  extern GROUP_t    *currentGroup;
  struct symtable_t *e = get_symbol(tok);
  char    floatbuf[17];
  void   *ptr = ((void*)currentGroup) + e->offset;
  u64    *z   = (u64*)ptr;
  int    *t   = (int*)   ptr;
  char   *s   = (char*)  ptr;
  u64     i   = 0;
  char   *c;
  float   f;
  int     j;

  ASSERT(e != NULL);

  ASSERT(e->sym_type == FIO_VARIABLE ||
	 e->sym_type == FIO_FLAG);

  BDEBUG("val %s tok %s\n", val, tok);  
  BDEBUG("sym entry %p, name %s, param_type %d\n", 
	e, e->name, e->sym_param_type);

  switch(e->sym_param_type) { 

  case FIO_STRING:    
    strncpy(s, val, e->param);
    BDEBUG("FIO_STRING Tok '%s' set to '%s'\n", 
	  e->name, s);
    break;

  case FIO_INT:
    *t = strtol(val, NULL, 10);
    BDEBUG("FIO_INT Tok '%s' set to '%d'\n", 
	  e->name, *t);
    break;

  case FIO_BOOL:
    if ( !strncmp("yes", val, 3) ) { 
      *t |= e->param;
      BDEBUG("FIO_BOOL Option '%s' enabled\n", e->name);
    } else { 
      BDEBUG("FIO_BOOL Option '%s' disabled\n", e->name);    
    }
    break;
    
  case FIO_FLOAT:
    f = atof(val);
    c = floatbuf;
    j = 0;   
    bzero(floatbuf, 16);
    snprintf(floatbuf, 16, "%f", f);    

    BDEBUG("float_val %f '%s'\n", 
	  f, floatbuf);

    while (*(c++) != '\0') 
      if (*c == '.') *c = '\0';

    *t = strtol(floatbuf, NULL, 10);
    
    if ( *c != '\0' ) { 
      *(t+1) = strtol(c, NULL, 10);
    }
    BDEBUG("FIO_FLOAT Tok '%s' Secs %d Usecs %d \n", 
	  e->name, *t, *(t+1));
    break;

  case FIO_SIZET:
    j = strlen(val);
    c = &val[j-1];
    
    switch(*c) { 
    case 'b':
    case 'B':
      i = (u64)1;
      break;
    case 'k':
    case 'K':
      i = (u64)1024;
      break;
    case 'm':
    case 'M':
      i = (u64)1024*1024;
      break;
    case 'g':
    case 'G':
      i = (u64)1024*1024*1024;
      break;
    case 't':
    case 'T':
      i = (u64)1024*1024*1024*1024;
      break;   
    default:
      WARN("Sizeval '%c' is not valid\n", *c);
      ASSERT(0);
    }

    BDEBUG("ival   = %llu \n", i);

    val[j-1] = '\0';
    *z = (u64)(i * strtoull(val, NULL, 10));
    
    BDEBUG("FIO_SIZET Tok '%s' set to '%llu'\n", 
	  e->name, (u64)*z);
    break;
    
  default:
    WARN("Invalid Token '%s'\n", e->name);
    ASSERT(0);
  }    
  
  return;
} 
