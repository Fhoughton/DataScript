#include "mpc.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt) {
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer) + 1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy) - 1] = '\0';
	return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

//Parser declerations (for builtin_load)
mpc_parser_t* Number;
mpc_parser_t* Symbol;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr;
mpc_parser_t* Datascript;

//Forward declarations
struct val;
struct env;
typedef struct val val;
typedef struct env env;

//Create enum of possible val types
enum { VAL_ERR, VAL_NUM, VAL_SYM, VAL_STR, VAL_FUN, VAL_SEXPR, VAL_QEXPR };

//To get a val* we dereference dsbuiltin and call it with a env* and a val*, therefore lbuiltin must be a function pointer that takes an env* and a val* and returns a val*.
typedef val*(*dsbuiltin)(env*, val*);

//Declare val struct
struct val {
	int type;
	long num;
	//Error and symbol types have some string data
	char* err;
	char* sym;
	char* str;

	//Functions
	dsbuiltin dsbuiltin;
	env* env; //Environment to store arguments
	val* formals; //
	val* body; //Function body expression

	//Count of and pointer to address of a list of "val*"
	int count;
	val** cell;
};

//Create a pointer to new number type val
val* val_num(long x) {
	val* v = malloc(sizeof(val));
	v->type = VAL_NUM;
	v->num = x;
	return v;
}

//Error handling function
val* val_err(char* fmt, ...) {
	val* v = malloc(sizeof(val));
	v->type = VAL_ERR;

	//Create a va list and initialize it
	va_list va;
	va_start(va, fmt);

	//Allocate 512 bytes of space
	v->err = malloc(512);

	//printf the error string with a maximum of 511 characters
	vsnprintf(v->err, 511, fmt, va);

	//Reallocate to number of bytes actually used
	v->err = realloc(v->err, strlen(v->err) + 1);

	//Free our va list
	va_end(va);

	return v;
}

//Create a pointer to a new symbol type val
val* val_sym(char* s) {
	val* v = malloc(sizeof(val));
	v->type = VAL_SYM;
	v->sym = malloc(strlen(s) + 1); //strlen + 1 because in C all strings are null terminated and removing breaks lots of things
	strcpy(v->sym, s);
	return v;
}

//Create a pointer to a new empty string val
val* val_str(char* s) {
	val* v = malloc(sizeof(val));
	v->type = VAL_STR;
	v->str = malloc(strlen(s) + 1);
	strcpy(v->str, s);
	return v;
}

val* val_builtin(dsbuiltin func) {
	val* v = malloc(sizeof(val));
	v->type = VAL_FUN;
	v->dsbuiltin = func;
	return v;
}

env* env_new(void);

//Create a pointer to a val containing an expression; this being a lambda.
val* val_lambda(val* formals, val* body) {
	val* v = malloc(sizeof(val));
	v->type = VAL_FUN;

	//Set dsuiltin to null
	v->dsbuiltin = NULL;

	//Build new environment
	v->env = env_new();

	//Set formals and body
	v->formals = formals;
	v->body = body;
	return v;
}

//Create a pointer to a new empty sexpr type val
val* val_sexpr(void) {
	val* v = malloc(sizeof(val));
	v->type = VAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

//Create a pointer to a new empty qexpression val
val* val_qexpr(void) {
	val* v = malloc(sizeof(val));
	v->type = VAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

void env_del(env* e);

void val_del(val* v) {

	switch (v->type) {

		case VAL_NUM: break;

		case VAL_FUN: 
		if (!v->dsbuiltin) {
			env_del(v->env); //Delete the environment
			val_del(v->formals); //Delete the formals
			val_del(v->body); //Delete the function body
		}
		break;

		//For Err or Sym free the string
		case VAL_ERR: free(v->err); break;
		
		case VAL_SYM: free(v->sym); break;

		case VAL_STR: free(v->str); break;

		//If Sexpr then delete all child elements inside
		case VAL_QEXPR: 
		case VAL_SEXPR:
		for (int i = 0; i < v->count; i++) {
			val_del(v->cell[i]);
		}
		free(v->cell); //Also free the memory allocated to contain the pointers
		break;
	}

	//Free the memory allocated for the val struct itself 
	free(v);
}

env* env_copy(env* e);

val* val_copy(val* v) {

	val* x = malloc(sizeof(val));
	x->type = v->type;

	switch (v->type) {

	//Copy functions and numbers directly
	case VAL_FUN:
	if (v->dsbuiltin) {
		x->dsbuiltin = v->dsbuiltin;
	}
	else {
		x->dsbuiltin = NULL;
		x->env = env_copy(v->env);
		x->formals = val_copy(v->formals);
		x->body = val_copy(v->body);
	}
	break;
	case VAL_NUM: x->num = v->num; break;

	//Copy error strings using malloc and strcpy
	case VAL_ERR:
		x->err = malloc(strlen(v->err) + 1);
		strcpy(x->err, v->err); break;

	//Copy symbols using malloc and strcpy
	case VAL_SYM:
		x->sym = malloc(strlen(v->sym) + 1);
		strcpy(x->sym, v->sym); break;

	//Copy strings using malloc and strcpy
	case VAL_STR: x->str = malloc(strlen(v->str) + 1);
		strcpy(x->str, v->str); break;

	//Copy lists by copying each sub-expression
	case VAL_QEXPR:
	case VAL_SEXPR:
		x->count = v->count;
		x->cell = malloc(sizeof(val*) * x->count);
		for (int i = 0; i < x->count; i++) {
			x->cell[i] = val_copy(v->cell[i]);
		}
		break;
	}

	return x;
}

val* val_add(val* v, val* x) {
	v->count++;
	v->cell = realloc(v->cell, sizeof(val*) * v->count);
	v->cell[v->count - 1] = x;
	return v;
}

//Child function of Join
val* val_join(val* x, val* y) {
	
	//For each cell in 'y' add it to 'x'
	for (int i = 0; i < y->count; i++) 
	{
		x = val_add(x, y->cell[i]);
	}
	
	//Delete the empty 'y' and return 'x'
	free(y->cell);
	free(y);
	return x;
}

val* val_pop(val* v, int i) {
	//Find the item at position i
	val* x = v->cell[i];

	//Shift memory after the item at i over the top
	memmove(&v->cell[i], &v->cell[i + 1], sizeof(val*) * (v->count - i - 1));

	//Decrease the count of items in the list
	v->count--;

	//Reallocate the memory used
	v->cell = realloc(v->cell, sizeof(val*) * v->count);
	
	//Return pointer to list with popped val
	return x;
}

val* val_take(val* v, int i) {
	val* x = val_pop(v, i);
	val_del(v);
	return x;
}

void val_print(val* v); //Forward decleration since val_expr_print and val_print reference eachother

void val_expr_print(val* v, char open, char close) {
	putchar(open);
	for (int i = 0; i < v->count; i++) {

		//Print Value contained within
		val_print(v->cell[i]);

		//Don't print trailing space if last element 
		if (i != (v->count - 1)) {
			putchar(' ');
		}
	}
	putchar(close);
}

void val_str_print(val* v) {
	//Make a copy of the string
	char* escaped = malloc(strlen(v->str) + 1);
	strcpy(escaped, v->str);
	//Pass it through the escape function
	escaped = mpcf_escape(escaped);
	//Print it between " characters
	printf("\"%s\"", escaped);
	//Free the copied string
	free(escaped);
}

//Print a val - A container for numbers, sexpressions, symbols and errors *MAKE IT SO IT PROVIDES POSITIONAL EXPLANATIONS FOR ERRORS USING THE AST*
void val_print(val* v) {
	switch (v->type) {
	case VAL_FUN:
		if (v->dsbuiltin) {
			printf("<builtin>");
		}
		else {
			printf("(lambda "); val_print(v->formals);
			putchar(' '); val_print(v->body); putchar(')');
		}
		break;
	case VAL_NUM:   printf("%li", v->num); break;
	case VAL_ERR:   printf("error: %s", v->err); break;
	case VAL_SYM:   printf("%s", v->sym); break;
	case VAL_STR:   val_str_print(v); break;
	case VAL_SEXPR: val_expr_print(v, '(', ')'); break;
	case VAL_QEXPR: val_expr_print(v, '{', '}'); break;
	}
}

//Print a val followed by a newline
void val_println(val* v) { val_print(v); putchar('\n'); }

//Body function for checking values are equal
int val_equal(val* x, val* y) {

	//Different types are always unequal
	if (x->type != y->type) { return 0; }

	//Compare based upon type
	switch (x->type) {
		//Compare number value
	case VAL_NUM: return (x->num == y->num);

		//Compare string values
	case VAL_ERR: return (strcmp(x->err, y->err) == 0);
	case VAL_SYM: return (strcmp(x->sym, y->sym) == 0);
	case VAL_STR: return (strcmp(x->str, y->str) == 0);

		//If builtin compare, otherwise compare formals and body
	case VAL_FUN:
		if (x->dsbuiltin || y->dsbuiltin) {
			return x->dsbuiltin == y->dsbuiltin;
		}
		else {
			return val_equal(x->formals, y->formals)
				&& val_equal(x->body, y->body);
		}

		//If list compare every individual element
	case VAL_QEXPR:
	case VAL_SEXPR:
		if (x->count != y->count) { return 0; }
		for (int i = 0; i < x->count; i++) {
			//If any element not equal then whole list not equal
			if (!val_equal(x->cell[i], y->cell[i])) { return 0; }
		}
		//Otherwise lists must be equal
		return 1;
		break;
	}
	return 0;
}

//Convert ENUMS to string names
char* type_name(int t) {
	switch (t) {
	case VAL_FUN: return "function";
	case VAL_NUM: return "number";
	case VAL_ERR: return "error";
	case VAL_SYM: return "symbol";
	case VAL_STR: return "string";
	case VAL_SEXPR: return "sexpression";
	case VAL_QEXPR: return "qexpression";
	default: return "unknown";
	}
}

struct env {
	env* par;
	int count;
	char** syms;
	val** vals;
};

//Create new environment
env* env_new(void) {
	env* e = malloc(sizeof(env));
	e->par = NULL;
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

//Delete an environment
void env_del(env* e) {
	for (int i = 0; i < e->count; i++) {
		free(e->syms[i]);
		val_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

env* env_copy(env* e) {
	env* n = malloc(sizeof(env));
	n->par = e->par;
	n->count = e->count;
	n->syms = malloc(sizeof(char*) * n->count);
	n->vals = malloc(sizeof(val*) * n->count);
	for (int i = 0; i < e->count; i++) {
		n->syms[i] = malloc(strlen(e->syms[i]) + 1);
		strcpy(n->syms[i], e->syms[i]);
		n->vals[i] = val_copy(e->vals[i]);
	}
	return n;
}

//Get a value from an environment
val* env_get(env* e, val* k) {

	//Iterate over all items in environment
	for (int i = 0; i < e->count; i++) {
		//Check if the stored string matches the symbol string
		//If it does, return a copy of the value
		if (strcmp(e->syms[i], k->sym) == 0) {
			return val_copy(e->vals[i]);
		}
	}

	//If no symbol check in parent otherwise error
	if (e->par) {
		return env_get(e->par, k);
	}
	else {
		return val_err("unbound Symbol '%s'", k->sym);
	}
}

//Copy environment val and symbols to new memory address
void env_put(env* e, val* k, val* v) {
	//Iterate over all items in environment
	//This is to see if variable already exists
	for (int i = 0; i < e->count; i++) {

		//If variable is found delete item at that position
		//And replace with variable supplied by user
		if (strcmp(e->syms[i], k->sym) == 0) {
			val_del(e->vals[i]);
			e->vals[i] = val_copy(v);
			return;
		}
	}

	//If no existing entry found allocate space for new entry
	e->count++;
	e->vals = realloc(e->vals, sizeof(val*) * e->count);
	e->syms = realloc(e->syms, sizeof(char*) * e->count);

	//Copy contents of val and symbol string into new location
	e->vals[e->count - 1] = val_copy(v);
	e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
	strcpy(e->syms[e->count - 1], k->sym);
}

void env_def(env* e, val* k, val* v) {
	//Iterate till e has no parent
	while (e->par) { e = e->par; }
	//Put value in e
	env_put(e, k, v);
}

//C Macros
#define ASSERT(args, cond, fmt, ...) \
  if (!(cond)) { val* err = val_err(fmt, ##__VA_ARGS__); val_del(args); return err; }

#define ASSERT_TYPE(func, args, index, expect) \
  ASSERT(args, args->cell[index]->type == expect, "function '%s' passed incorrect type for argument %i; got %s, expected %s.", func, index, type_name(args->cell[index]->type), type_name(expect))

#define ASSERT_TYPE_DOUBLE(func, args, index, expect, expect2) \
  ASSERT(args, args->cell[index]->type == expect ||  args->cell[index]->type == expect2, "function '%s' passed incorrect type for argument %i; got %s, expected %s or %s.", func, index, type_name(args->cell[index]->type), type_name(expect), type_name(expect2))


#define ASSERT_NUM(func, args, num) \
  ASSERT(args, args->count == num, "function '%s' passed incorrect number of arguments; got %i, expected %i.", func, args->count, num)

#define ASSERT_NOT_EMPTY(func, args, index) \
  ASSERT(args, args->cell[index]->count != 0, "function '%s' passed {} for argument %i.", func, index);

val* val_eval(env* e, val* v);

//Lambda function, used for defining expressions
val* builtin_lambda(env* e, val* a) {
	//Check two arguments, each of which are qexpressions
	ASSERT_NUM("lambda", a, 2);
	ASSERT_TYPE("lambda", a, 0, VAL_QEXPR);
	ASSERT_TYPE("lambda", a, 1, VAL_QEXPR);

	//Check first qexpression contains only symbols
	for (int i = 0; i < a->cell[0]->count; i++) {
		ASSERT(a, (a->cell[0]->cell[i]->type == VAL_SYM), "cannot define non-symbol. Got %s, Expected %s.", type_name(a->cell[0]->cell[i]->type), type_name(VAL_SYM));
	}

	//Pop first two arguments and pass them to val_lambda
	val* formals = val_pop(a, 0);
	val* body = val_pop(a, 0);
	val_del(a);

	return val_lambda(formals, body);
}

val* builtin_fun(env* e, val* a)
{
	ASSERT_NUM("if", a, 3);
	ASSERT_TYPE("if", a, 0, VAL_NUM);
	ASSERT_TYPE("if", a, 1, VAL_QEXPR);
	ASSERT_TYPE("if", a, 2, VAL_QEXPR);

	//Mark both expressions as evaluateable
	val* x;
	a->cell[1]->type = VAL_SEXPR;
	a->cell[2]->type = VAL_SEXPR;

	if (a->cell[0]->num) {
		//If condition is true evaluate first expression
		x = val_eval(e, val_pop(a, 1));
	}
	else {
		//Otherwise evaluate second expression
		x = val_eval(e, val_pop(a, 2));
	}

	//Delete argument list and return
	val_del(a);
	return x;
}

//List function - Converts input S-Expression into a Q-Expression and returns it
val* builtin_list(env* e, val* a) {
	a->type = VAL_QEXPR;
	return a;
}

//Body function - Returns list without first and last item, useful for parsing
val* builtin_body(env* e, val* a) {
	//Too many arguments
	ASSERT_NUM("body", a, 1);

	//Incorrect types
	ASSERT_TYPE("body", a, 0, VAL_QEXPR);

	//{} passed to function
	ASSERT_NOT_EMPTY("body", a, 0);

	//Otherwise take first argument
	val* v = val_take(a, 0);

	//Delete head and tail and then return
	val_del(val_pop(v, 0));
	val_del(val_pop(v, v->count-1));
	return v;
}

//Head function - Returns first item in list
val* builtin_head(env* e, val* a) {
	//Too many arguments
	ASSERT_NUM("head", a, 1);

	//Incorrect types
	ASSERT_TYPE("head", a, 0, VAL_QEXPR);

	//{} passed to function
	ASSERT_NOT_EMPTY("head", a, 0);

	//Otherwise take first argument
	val* v = val_take(a, 0);

	//Delete all elements that are not head and return
	while (v->count > 1) { val_del(val_pop(v, 1)); }
	return v;
}

//Tail function - returns all but first item of function
val* builtin_tail(env* e, val* a) {

	//Too many arguments
	ASSERT_NUM("tail", a, 1);
	
	//Incorrect types
	ASSERT_TYPE("tail", a, 0, VAL_QEXPR);
	
	//{} passed to function
	ASSERT_NOT_EMPTY("tail", a, 0);

	//Take first argument
	val* v = val_take(a, 0);

	//Delete first element and return
	val_del(val_pop(v, 0));
	return v;
}

//Eval function - Take a Q-Expression and evaluate it as an S-Expression to get the result
val* builtin_eval(env* e, val* a) {
	ASSERT_NUM("eval", a, 1);
	ASSERT_TYPE("eval", a, 0, VAL_QEXPR);

	val* x = val_take(a, 0);
	x->type = VAL_SEXPR;
	return val_eval(e, x);
}

//Pop function - Removes a selected item by index from a list
val* builtin_pop(env* e, val* a) {
	//Too many/few arguments
	ASSERT_NUM("pop", a, 2);

	//Incorrect types
	ASSERT_TYPE("pop", a, 0, VAL_QEXPR);
	ASSERT_TYPE("pop", a, 1, VAL_NUM);

	//{} passed to function
	ASSERT_NOT_EMPTY("pop", a, 0);

	val* x = val_pop(a, 0); //List
	val* y = val_pop(a, 0); //Pop index

	if (y->num <= x->count)
	{
		val_pop(x, y->num);
	}

	val_del(a);
	return x;
}

//Pop function - Removes a selected item by index from a list
val* builtin_len(env* e, val* a) {
	//Too many/few arguments
	ASSERT_NUM("len", a, 1);

	//{} passed to function
	ASSERT_NOT_EMPTY("len", a, 0);

	val* x = val_pop(a, 0); //Supplied argument to find length of

	switch (x->type) 
	{
		case VAL_QEXPR: case VAL_SEXPR: 
		{
			x = val_num(x->count);
			break;
		}
		case VAL_STR:
		{
			x = val_num(strlen(x->str));
			break;
		}
		case VAL_NUM:
		{
			char buffer[sizeof(x->num) * 8 + 1]; 
			x = val_num(strlen(_ultoa(x->num, buffer, 10)));
			break;
		}
	}

	val_del(a);
	return x;
}

//Fetch function - Returns selected value from a list
val* builtin_fetch(env* e, val* a) {
	//Too many/few arguments
	ASSERT_NUM("fetch", a, 2);

	//Incorrect types
	ASSERT_TYPE("fetch", a, 0, VAL_QEXPR);
	ASSERT_TYPE("fetch", a, 1, VAL_NUM);

	//{} passed to function
	ASSERT_NOT_EMPTY("fetch", a, 0);

	val* x = val_pop(a, 0); //List
	val* y = val_pop(a, 0); //Fetched index

	if (y->num <= x->count)
	{
		x = val_take(x,y->num);
	}
	else 
	{
		x = val_err("invalid index");
	}

	val_del(a);
	return x;
}

//Join function - Joins multiple Q-Expressions into one
val* builtin_join(env* e, val* a) {

	for (int i = 0; i < a->count; i++) {
		ASSERT_TYPE("join", a, i, VAL_QEXPR);
	}

	val* x = val_pop(a, 0);

	while (a->count) {
		val* y = val_pop(a, 0);
		x = val_join(x, y);
	}

	val_del(a);
	return x;
}

//Head function - Returns first item in list
val* builtin_typeof(env* e, val* a) {
	//Too many arguments
	ASSERT_NUM("typeof", a, 1);

	//{} passed to function
	ASSERT_NOT_EMPTY("typeof", a, 0);

	//Otherwise take first argument
	val* v = val_take(a, 0);
	
	return val_num(v->type);
}

//Head function - Returns first item in list
val* builtin_typename(env* e, val* a) {
	//Too many arguments
	ASSERT_NUM("type_name", a, 1);

	//{} passed to function
	ASSERT_NOT_EMPTY("type_name", a, 0);

	//Otherwise take first argument
	val* v = val_take(a, 0);

	return val_str(type_name(v->num));
}


//Builtin operands, (+,-,*,/)
val* builtin_op(env* e, val* a, char* op) {

	//Ensure all arguments are numbers
	for (int i = 0; i < a->count; i++) {
		ASSERT_TYPE(op, a, i, VAL_NUM);
	}

	//Pop the first element
	val* x = val_pop(a, 0);

	//If no arguments and sub then perform negation
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		x->num = -x->num;
	}

	//While there are still elements remaining
	while (a->count > 0) {

		//Pop the next element
		val* y = val_pop(a, 0);

		if (strcmp(op, "+") == 0) { if (x->type == VAL_NUM) { x->num += y->num; } else if (x->type == VAL_STR) { x->str = (strcat(x->str,y->str)); }; }
		if (strcmp(op, "-") == 0) { x->num -= y->num; }
		if (strcmp(op, "*") == 0) { x->num *= y->num; }
		if (strcmp(op, "/") == 0) {
			if (y->num == 0) {
				val_del(x);
				val_del(y);
				x = val_err("Division By Zero.");
				break;
			}
			x->num /= y->num;
		}
		val_del(y);
	}
	val_del(a);
	return x;
}

//Mathematics
val* builtin_add(env* e, val* a) {

	//Ensure all arguments are numbers
	for (int i = 0; i < a->count; i++) {
		ASSERT_TYPE_DOUBLE("+", a, i, VAL_NUM, VAL_STR);
	}

	//Pop the first element
	val* x = val_pop(a, 0);

	//While there are still elements remaining
	while (a->count > 0) {

		//Pop the next element
		val* y = val_pop(a, 0);

		switch (x->type)
		{
			case VAL_STR:
			{
				char * ystr;

				if (y->type == VAL_NUM) { char buffer[sizeof(y->num) * 8 + 1]; ystr = _ultoa(y->num, buffer, 10); } else { ystr = y->str; }

				char* s = strcat(x->str, ystr);
				x->type = VAL_STR;
				x->str = malloc(strlen(s) + 1);
				x->str = strcpy(x->str, s);
				break;
			}

			case VAL_NUM:
			{
				long ynum;

				if (y->type == VAL_STR) 
				{ 
					char* modstring = y->str;

					//Remove all non number characters from the string
					int j = 0;

					for (int i = 0; modstring[i]; i++) {
						if (modstring[i] >= '0' && modstring[i] <= '9') {
							modstring[j] = modstring[i];
							j++;
						}
					}

					modstring[j] = '\0';

					//Convert string to long
					char *ptr; 
					ynum = strtol(modstring,&ptr, 10); 
				}
				else 
				{
					ynum = y->num;
				}

				x->num += ynum;
			}
		}

		val_del(y);
	}
	val_del(a);
	return x;
}

val* builtin_sub(env* e, val* a) {
	return builtin_op(e, a, "-");
}

val* builtin_mul(env* e, val* a) {
	return builtin_op(e, a, "*");
}

val* builtin_div(env* e, val* a) {
	return builtin_op(e, a, "/");
}

val* builtin_var(env* e, val* a, char* func) {
	ASSERT_TYPE(func, a, 0, VAL_QEXPR);

	val* syms = a->cell[0];
	for (int i = 0; i < syms->count; i++) {
		ASSERT(a, (syms->cell[i]->type == VAL_SYM), "function '%s' cannot define non-symbol; got %s, expected %s.", func, type_name(syms->cell[i]->type), type_name(VAL_SYM));
	}

	ASSERT(a, (syms->count == a->count - 1), "function '%s' passed too many arguments for symbols; got %i, expected %i.", func, syms->count, a->count - 1);

	for (int i = 0; i < syms->count; i++) {
		//If 'var' define in globally. If 'putvar' define in locally
		if (strcmp(func, "def") == 0) {
			env_def(e, syms->cell[i], a->cell[i + 1]);
		}

		if (strcmp(func, "=") == 0) {
			env_put(e, syms->cell[i], a->cell[i + 1]);
		}
	}

	val_del(a);
	return val_sexpr();
}

val* builtin_def(env* e, val* a) {
	return builtin_var(e, a, "def");
}

val* builtin_put(env* e, val* a) {
	return builtin_var(e, a, "=");
}

//Conditonal controller
val* builtin_ord(env* e, val* a, char* op) {
	ASSERT_NUM(op, a, 2);
	ASSERT_TYPE(op, a, 0, VAL_NUM);
	ASSERT_TYPE(op, a, 1, VAL_NUM);

	int r;
	if (strcmp(op, ">") == 0) {
		r = (a->cell[0]->num > a->cell[1]->num);
	}
	if (strcmp(op, "<") == 0) {
		r = (a->cell[0]->num < a->cell[1]->num);
	}
	if (strcmp(op, ">=") == 0) {
		r = (a->cell[0]->num >= a->cell[1]->num);
	}
	if (strcmp(op, "<=") == 0) {
		r = (a->cell[0]->num <= a->cell[1]->num);
	}
	val_del(a);
	return val_num(r);
}

//Conditionals
val* builtin_greater(env* e, val* a) {
	return builtin_ord(e, a, ">");
}

val* builtin_less(env* e, val* a) {
	return builtin_ord(e, a, "<");
}

val* builtin_greaterorequal(env* e, val* a) {
	return builtin_ord(e, a, ">=");
}

val* builtin_lessorequal(env* e, val* a) {
	return builtin_ord(e, a, "<=");
}

val* builtin_compare(env* e, val* a, char* op) {
	ASSERT_NUM(op, a, 2);
	int r;
	if (strcmp(op, "==") == 0) {
		r = val_equal(a->cell[0], a->cell[1]);
	}
	if (strcmp(op, "!=") == 0) {
		r = !val_equal(a->cell[0], a->cell[1]);
	}
	val_del(a);
	return val_num(r);
}

val* builtin_equal(env* e, val* a) {
	return builtin_compare(e, a, "==");
}

val* builtin_notequal(env* e, val* a) {
	return builtin_compare(e, a, "!=");
}

val* builtin_if(env* e, val* a) {
	ASSERT_NUM("if", a, 3);
	ASSERT_TYPE("if", a, 0, VAL_NUM);
	ASSERT_TYPE("if", a, 1, VAL_QEXPR);
	ASSERT_TYPE("if", a, 2, VAL_QEXPR);

	//Mark both expressions as evaluateable
	val* x;
	a->cell[1]->type = VAL_SEXPR;
	a->cell[2]->type = VAL_SEXPR;

	if (a->cell[0]->num) {
		//If condition is true evaluate first expression
		x = val_eval(e, val_pop(a, 1));
	}
	else {
		//Otherwise evaluate second expression
		x = val_eval(e, val_pop(a, 2));
	}

	//Delete argument list and return
	val_del(a);
	return x;
}

val* builtin_while(env* e, val* a) {
	ASSERT_NUM("while", a, 2);
	ASSERT_TYPE("while", a, 0, VAL_NUM);
	ASSERT_TYPE("while", a, 1, VAL_QEXPR);

	//Mark both expressions as evaluateable
	val* x;
	a->cell[1]->type = VAL_SEXPR;

	x = val_pop(a, 1);

	while (a->cell[0]->num) {
		//If condition is true evaluate first expression
		val_print(val_eval(e, x));
	}

	//Delete argument list and return
	val_del(a);
	return x;
}

val* builtin_loop(env* e, val* a) {
	ASSERT_NUM("loop", a, 2);
	ASSERT_TYPE("loop", a, 0, VAL_NUM);
	ASSERT_TYPE("loop", a, 1, VAL_QEXPR);

	//Mark expression as evaluateable
	val* x = val_err("Something went wrong!!");
	a->cell[1]->type = VAL_SEXPR;

	val* y = val_pop(a, 1);

	for (int i = 0; i < a->cell[0]->num; i++) {
		//If condition is true evaluate first expression
		x = val_eval(e, y);
	}

	//Delete argument list and return
	val_del(a);
	return x;
}

val* val_read(mpc_ast_t* t);

//Range function - Generates a qexpression containing all the number in a certain range of numbers
val* builtin_range(env* e, val* a) {
	//Too many arguments
	ASSERT_NUM("range", a, 2);
	ASSERT_TYPE("range", a, 0, VAL_NUM);
	ASSERT_TYPE("range", a, 1, VAL_NUM);

	//{} passed to function
	ASSERT_NOT_EMPTY("range", a, 0);

	//Otherwise take first and second argument
	val* v = val_take(a, 0);
	val* w = val_take(a, 0);

	val* x = val_num(10);

	if (v->num < w->num) 
	{
		//Count up if second argument greater than first
		for (int i = v->num; i < w->num; i++) 
		{
			x = val_qexpr(); //Somehow insert i into each slot :thinking:
			x = val_add(x, val_num(i));
		}
	}
	else
	{
		if (v->num > w->num) 
		{
			//Count down if second argument less than first
			for (int i = v->num; i < w->num; i--)
			{
				x = val_qexpr(); //Somehow insert i into each slot :thinking:
				x = val_add(x, val_num(i));
			}
		}
		else 
		{
			//Must be equal so just return the first number
			x = v;
		}
	}

	return x;
}

val* builtin_load(env* e, val* a) {
	ASSERT_NUM("load", a, 1);
	ASSERT_TYPE("load", a, 0, VAL_STR);

	//Parse file given by string name
	mpc_result_t r;
	if (mpc_parse_contents(a->cell[0]->str, Datascript, &r)) {

		//Read content
		val* expr = val_read(r.output);
		mpc_ast_delete(r.output);

		//Evaluate each expression
		while (expr->count) {
			val* x = val_eval(e, val_pop(expr, 0));
			//If Evaluation leads to error print it
			if (x->type == VAL_ERR) { val_println(x); }
			val_del(x);
		}

		//Delete expressions and arguments
		val_del(expr);
		val_del(a);

		//Return empty list
		return val_sexpr();

	}
	else {
		//Get parse error as string
		char* err_msg = mpc_err_string(r.error);
		mpc_err_delete(r.error);

		//Create new error message using it
		val* err = val_err("could not load Library %s", err_msg);
		free(err_msg);
		val_del(a);

		//Cleanup and return error
		return err;
	}
}

val* builtin_print(env* e, val* a) {

	//Print each argument followed by a space
	for (int i = 0; i < a->count; i++) {
		val_print(a->cell[i]); putchar(' ');
	}

	//Delete arguments
	val_del(a);

	return val_sexpr();
}

val* builtin_println(env* e, val* a) {

	//Print each argument followed by a space
	for (int i = 0; i < a->count; i++) {
		val_print(a->cell[i]); putchar(' ');
	}

	//Print a newline and delete arguments
	putchar('\n');
	val_del(a);

	return val_sexpr();
}

val* builtin_read(env* e, val* a) {
	ASSERT_NUM("read", a, 1);
	ASSERT_TYPE("read", a, 0, VAL_STR);

	//val* v = val_take(a, 0);
	val* x = NULL;

	//Print each argument followed by a space
	printf(a->cell[0]->str);
	char* input = readline("");
	x = val_str(input);

	//Delete arguments
	val_del(a);

	return x;
}

void env_add_builtin(env* e, char* name, dsbuiltin func) {
	val* k = val_sym(name);
	val* v = val_builtin(func);
	env_put(e, k, v);
	val_del(k);
	val_del(v);
}

void env_add_builtins(env* e) {
	//Core functions
	env_add_builtin(e, "lambda", builtin_lambda);
	env_add_builtin(e, "=", builtin_def);
	env_add_builtin(e, "put", builtin_put);
	env_add_builtin(e, "load", builtin_load);
	env_add_builtin(e, "loop", builtin_loop);

	//Type names
	env_add_builtin(e, "typeof", builtin_typeof);
	env_add_builtin(e, "type_name", builtin_typename);

	//Output functions
	env_add_builtin(e, "print", builtin_print);
	env_add_builtin(e, "println", builtin_println);
	env_add_builtin(e, "read", builtin_read);

	//List functions
	env_add_builtin(e, "list", builtin_list);
	env_add_builtin(e, "head", builtin_head);
	env_add_builtin(e, "body", builtin_body);
	env_add_builtin(e, "tail", builtin_tail);
	env_add_builtin(e, "pop", builtin_pop);
	env_add_builtin(e, "len", builtin_len);
	env_add_builtin(e, "fetch", builtin_fetch);
	env_add_builtin(e, "eval", builtin_eval);
	env_add_builtin(e, "join", builtin_join);
	env_add_builtin(e, "range", builtin_range);

	//Mathematical functions
	env_add_builtin(e, "+", builtin_add);
	env_add_builtin(e, "-", builtin_sub);
	env_add_builtin(e, "*", builtin_mul);
	env_add_builtin(e, "/", builtin_div);

	//Comparison functions
	env_add_builtin(e, "if", builtin_if);
	env_add_builtin(e, "while", builtin_while);
	env_add_builtin(e, "==", builtin_equal);
	env_add_builtin(e, "!=", builtin_notequal);
	env_add_builtin(e, ">", builtin_greater);
	env_add_builtin(e, "<", builtin_less);
	env_add_builtin(e, ">=", builtin_greaterorequal);
	env_add_builtin(e, "<=", builtin_lessorequal);
}

val* val_call(env* e, val* f, val* a) {

	//If builtin then simply apply that
	if (f->dsbuiltin) { return f->dsbuiltin(e, a); }

	//Record argument counts
	int given = a->count;
	int total = f->formals->count;

	//While arguments still remain to be processed
	while (a->count) {

		//If we've ran out of formal arguments to bind
		if (f->formals->count == 0) {
			val_del(a); return val_err("function passed too many arguments; got %i, expected %i.", given, total);
		}

		//Pop the first symbol from the formals
		val* sym = val_pop(f->formals, 0);

		//Special Case to deal with '&'
		if (strcmp(sym->sym, "&") == 0) {

			//Ensure '&' is followed by another symbol
			if (f->formals->count != 1) {
				val_del(a);
				return val_err("function format invalid; symbol '&' not followed by single symbol.");
			}

			//Next formal should be bound to remaining arguments
			val* nsym = val_pop(f->formals, 0);
			env_put(f->env, nsym, builtin_list(e, a));
			val_del(sym);
			val_del(nsym);
			break;
		}

		//Pop the next argument from the list
		val* val = val_pop(a, 0);

		//Bind a copy into the function's environment
		env_put(f->env, sym, val);

		//Delete symbol and value
		val_del(sym);
		val_del(val);
	}

	//Argument list is now bound so can be cleaned up
	val_del(a);

	//If '&' remains in formal list bind to empty list
	if (f->formals->count > 0 &&
		strcmp(f->formals->cell[0]->sym, "&") == 0) {

		//Check to ensure that & is not passed invalidly
		if (f->formals->count != 2) {
			return val_err("Function format invalid; symbol '&' not followed by single symbol.");
		}

		//Pop and delete '&' symbol
		val_del(val_pop(f->formals, 0));

		//Pop next symbol and create empty list
		val* sym = val_pop(f->formals, 0);
		val* val = val_qexpr();

		//Bind to environment and delete
		env_put(f->env, sym, val);
		val_del(sym);
		val_del(val);
	}

	//If all formals have been bound evaluate
	if (f->formals->count == 0) {

		//Set environment parent to evaluation environment
		f->env->par = e;

		//Evaluate and return
		return builtin_eval(f->env, val_add(val_sexpr(), val_copy(f->body)));
	}
	else {
		//Otherwise return partially evaluated function
		return val_copy(f);
	}
}

val* val_eval_sexpr(env* e, val* v) {

	//Evaluate children
	for (int i = 0; i < v->count; i++) {
		v->cell[i] = val_eval(e, v->cell[i]);
	}

	//Error Checking
	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == VAL_ERR) { return val_take(v, i); }
	}

	//Empty expression
	if (v->count == 0) { return v; }

	//Single expression
	if (v->count == 1) { return val_take(v, 0); }

	//Ensure first element is a function after evaluation
	val* f = val_pop(v, 0);
	if (f->type != VAL_FUN)
	{
		val* err = val_err("sexpression starts with incorrect type; got %s, expected %s.", type_name(f->type), type_name(VAL_FUN));
		val_del(f);
		val_del(v);
		return err;
	}

	//Call builtin with operator
	val* result = val_call(e, f, v);

	val_del(f);
	return result;
}

val* val_eval(env* e, val* v) {
	if (v->type == VAL_SYM) {
		val* x = env_get(e, v);
		val_del(v);
		return x;
	}
	if (v->type == VAL_SEXPR) { return val_eval_sexpr(e, v); }
	return v;
}

//Read a number and return pointer to long with value
val* val_read_num(mpc_ast_t* t) {
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE ? val_num(x) : val_err("invalid Number.");
}

//Read a string and return a pointer to string with value
val* val_read_str(mpc_ast_t* t) {
	//Cut off the final quote character
	t->contents[strlen(t->contents) - 1] = '\0';
	//Copy the string missing out the first quote character
	char* unescaped = malloc(strlen(t->contents + 1) + 1);
	strcpy(unescaped, t->contents + 1);
	//Pass through the unescape function
	unescaped = mpcf_unescape(unescaped);
	//Construct a new val using the string
	val* str = val_str(unescaped);
	//Free the string and return
	free(unescaped);
	return str;
}

val* val_read(mpc_ast_t* t) {

	//If symbol or number return conversion
	if (strstr(t->tag, "number")) { return val_read_num(t); }
	if (strstr(t->tag, "string")) { return val_read_str(t); }
	if (strstr(t->tag, "symbol")) { return val_sym(t->contents); }

	//If root (>) or sexpr then create empty list
	val* x = NULL;
	if (strcmp(t->tag, ">") == 0) { x = val_sexpr(); }
	if (strstr(t->tag, "sexpr")) { x = val_sexpr(); }
	if (strstr(t->tag, "qexpr")) { x = val_qexpr(); }

	//Fill above list with any valid expression contained within
	for (int i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
		if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
		if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
		if (strstr(t->children[i]->tag, "comment")) { continue; }
		x = val_add(x, val_read(t->children[i]));
	}

	return x;
}


//Main repl function
int main(int argc, char** argv)
{
	/*PARSING*/

	//Create parsers
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* String = mpc_new("string");
	mpc_parser_t* Comment = mpc_new("comment");
	mpc_parser_t* Sexpr = mpc_new("sexpr");
	mpc_parser_t* Qexpr = mpc_new("qexpr");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* DataScript = mpc_new("datascript");

	mpca_lang(MPCA_LANG_DEFAULT,
	"                                              \
      number  : /-?[0-9]+/ ;                       \
      symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
      string  : /\"(\\\\.|[^\"])*\"/ ;             \
      comment : /;[^\\r\\n]*/ ;                    \
      sexpr   : '(' <expr>* ')' ;                  \
      qexpr   : '{' <expr>* '}' ;                  \
      expr    : <number>  | <symbol> | <string>    \
              | <comment> | <sexpr>  | <qexpr>;    \
      datascript   : /^/ <expr>* /$/ ;             \
    ",
		Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, DataScript);

	/*CONSOLE OUTPUT*/
	//Initialise environment
	env* e = env_new();
	env_add_builtins(e);

	//REPL (read-evaluate-print loop); Used as command line interface;
	while (1) //LOOP
	{
		//READ
		char* input = readline("> ");
		add_history(input);

		//printf("No you're a %s", input); //Echoes back user input for testing

		//EVALUATE/PRINT
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, DataScript, &r)) {
			//On success print the Evaluation
			//AST - DEBUG
			//mpc_ast_print(r.output);
			//mpc_ast_delete(r.output);

			val* x = val_eval(e, val_read(r.output));
			val_println(x);
			val_del(x);
			mpc_ast_delete(r.output);
		}
		else {
			//Otherwise print the error
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input);
	}

	//Command line arguments
	//Supplied with list of files
	if (argc >= 2) {
		//Loop over each supplied filename (starting from 1)
		for (int i = 1; i < argc; i++) {

			//Argument list with a single argument, the filename
			val* args = val_add(val_sexpr(), val_str(argv[i]));

			//Pass to builtin load and get the result
			val* x = builtin_load(e, args);

			//If the result is an error be sure to print it
			if (x->type == VAL_ERR) { val_println(x); }
			val_del(x);
		}
	}

	//Destroy environment
	env_del(e);

	//Undefine and delete parsers
	mpc_cleanup(8, Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, DataScript);

	return 0;
}