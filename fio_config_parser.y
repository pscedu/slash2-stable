%{
#define YYSTYPE char *
#include "fio.h"
#include "fio_sym.h"
%}

%start group_blocks

%token END
%token EQ

%token IOTESTS

%token GROUP
%token GROUP_START
%token GROUP_END

%token SUB
%token SUBSECT_START
%token SUBSECT_END

%token RECIPE_SEP
%token RECIPE_END
%token RECIPE_START

%token NUM
%token NAME
%token PATHNAME
%token BOOL
%token SIZEVAL
%token FLOATVAL

%%
group_blocks : /* NULL */               |
               group_blocks group_block
{};

group_block : group_start statements GROUP_END 
{
  ASSERT(currentGroup->num_pes);

#ifdef PTHREADS
  BDEBUG("Allocating %d Thread Structs\n",
	 currentGroup->num_pes);  

  currentGroup->threads = malloc(sizeof(THREAD_t) *
				 currentGroup->num_pes);
#endif

  BDEBUG("Group '%s' processing complete\n",
	currentGroup->test_name);
};

group_start : GROUP NAME GROUP_START
{   
  extern GROUP_t *currentGroup;
  GROUP_t        *group;
  char *ptr = (char *)$2;

  if (!numGroups) {
    BDEBUG("initialize groupList\n");
    INIT_LIST_HEAD(&groupList);    
  }
  numGroups++;

  BDEBUG("malloc'ing %d bytes for testGroup inc_ptr %p\n",
	sizeof(GROUP_t), (void *)ptr);  

  BDEBUG("\nGroup Name = '%s'\n",
	 (char *)$2);

  group = malloc(sizeof(GROUP_t));
  ASSERT(group != NULL);
  bzero(group, sizeof(GROUP_t));
  INIT_LIST_HEAD(&group->group_list);
  list_add_tail(&group->group_list, &groupList);
  strncpy(group->test_name, (char *)$2, TEST_GROUP_NAME_MAX);
  currentGroup = group;

  BDEBUG("New Group Declared: Name '%s' Addr %p\n", 
	group->test_name, group);
};

statements        : /* NULL */               |
                    statement    statements  |
                    subsect_block statements ;

statement         : val_stmt   |
                    path_stmt  |
                    bool_stmt  |
                    size_stmt  |
                    float_stmt ;

val_stmt  : NAME EQ NAME END
{ 
  BDEBUG("Found Name/Val Statement: Tok '%s' Val '%s'\n",
	$1, $3);
  store_tok_val($1, $3);
};

path_stmt : NAME EQ PATHNAME END 
{ 
  BDEBUG("Found Path Statement: Tok '%s' Val '%s'\n",
	$1, $3);
  store_tok_val($1, $3);
};

bool_stmt : NAME EQ BOOL END
{ 
  BDEBUG("Found Bool Statement: Tok '%s' Val '%s'\n",
	$1, $3);
  store_tok_val($1, $3);
};

size_stmt  : NAME EQ SIZEVAL END
{ 
  BDEBUG("Found Sizeval Statement: Tok '%s' Val '%s'\n",
	$1, $3);
  store_tok_val($1, $3);
};

float_stmt : NAME EQ FLOATVAL END
{ 
  BDEBUG("Found Float Statement: Tok '%s' Val '%s'\n",
	$1, $3);
  store_tok_val($1, $3);
};


subsect_block     : subsect_begin statements   SUBSECT_END |
                    iotests SUBSECT_END 
{};

subsect_begin     : SUB NAME SUBSECT_START 
{ 
  struct symtable_t *sym = get_symbol((char *) $2);
  
  if (sym == NULL) { 
    WARN("Invalid config entry '%s'\n", $2);
    exit(1);
  }
};

iotests           : IOTESTS SUBSECT_START test_recipes
{};


test_recipes      : /* NULL */               | 
                    test_recipe test_recipes 
{};


test_recipe       : test_recipe_decl test_recipe_comps RECIPE_END 
{ 
  currentGroup->num_iotests++;
};


test_recipe_decl  : NAME RECIPE_START 
{
  extern GROUP_t *currentGroup;
  int num_iotests          =  currentGroup->num_iotests; 
  struct io_routine_t *ior = &currentGroup->iotests[num_iotests];

  strncpy(ior->io_testname, (char *)$1, 
	  TEST_GROUP_NAME_MAX);
  BDEBUG("Got test name '%s'\n", ior->io_testname);
};


test_recipe_comps : /* NULL */                          |
                    test_recipe_comp  test_recipe_comps |
		    test_recipe_scomp test_recipe_comps
{};

test_recipe_scomp  : NAME 
{ 
  store_func($1);
};


test_recipe_comp  : NAME RECIPE_SEP 
{
  store_func($1);
};


%%
int run_yacc()
{
  yyparse();
  return 0;
}

int yyerror(char *msg)
{
  printf("Error encountered: %s \n", msg);
  return 0;
}
