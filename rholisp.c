#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>

#include <ffi.h>

typedef int64_t i64;
typedef uint64_t u64;

#define da_append_many(da, things, amount) do { \
		if ((da)->capacity == 0) { \
			assert((da)->items == NULL); \
			(da)->items = malloc(16*sizeof(*(da)->items)); \
			(da)->count = 0; \
			(da)->capacity = 16; \
		} \
		while ((amount) + (da)->count > (da)->capacity) { \
			(da)->capacity *= 2; \
			/* TODO: unnecessary reallocs here, optimise? */ \
			(da)->items = realloc((da)->items, (da)->capacity); \
		} \
		if ((amount) == 0) break; \
		if ((amount) == 1) { \
			(da)->items[(da)->count++] = *(things); \
		} else { \
			memcpy(&(da)->items[(da)->count], (things), (amount)); \
			(da)->count += (amount); \
		} \
	} while (0)
#define da_append(da, thing) do { \
		if ((da)->capacity == 0) { \
			assert((da)->items == NULL); \
			(da)->items = malloc(16*sizeof(*(da)->items)); \
			(da)->count = 0; \
			(da)->capacity = 16; \
		} \
		if (1 + (da)->count > (da)->capacity) { \
			(da)->capacity *= 2; \
			(da)->items = realloc((da)->items, (da)->capacity); \
		} \
		(da)->items[(da)->count++] = (thing); \
	} while (0)
#define da_free(da) do { \
		if ((da)->capacity) { \
			free((da)->items); \
			(da)->count = 0; \
			(da)->capacity = 0; \
		} \
	} while (0)
#define da_foreach(da, type, it) for (type *it = (da)->items; it < (da)->items + (da)->count; ++it)

struct string_builder {
	char *items;
	size_t count;
	size_t capacity;
};
void sb_addb(struct string_builder *sb, const char *b, size_t l) {
	da_append_many(sb, b, l);
}
void sb_adds(struct string_builder *sb, const char *s) {
	sb_addb(sb, s, strlen(s));
}
void sb_addc(struct string_builder *sb, char c) {
	da_append(sb, c);
}
void sb_clear(struct string_builder *sb) {
	da_free(sb);
}

/* SECTION: INPUT */

struct string_builder line_builder = {0};
size_t line_len;
char *line;

enum input_status {
	IS_SUCCESS,
	IS_EOF,
};
enum input_status input(struct string_builder *line_builder, FILE *infile) {
	line_builder->count = 0;

	bool eof = 0;

	for (;;) {
		const int inp = getc(infile);

		if (inp == EOF) {
			eof = true;
			break;
		}

		if (inp == '\n') {
			break;
		}

		sb_addc(line_builder, inp);
	}

	return eof ? IS_EOF : IS_SUCCESS;
}

/* SECTION: LISP TYPES AND VALUES (DECLARATION) */

#define LIST_OF_LISP_TYPES     \
	X(number,  LT_NUMBER)  \
	X(builtin, LT_BUILTIN) \
	X(symbol,  LT_SYMBOL)  \
	X(list,    LT_LIST)    \
	X(boolean, LT_BOOLEAN) \
	X(string,  LT_STRING)
#define LIST_OF_GCD_TYPES    \
	X(symbol, LT_SYMBOL) \
	X(list,   LT_LIST)   \
	X(string, LT_STRING)

// the types said value can take on
enum lisp_type {
#define X(_, enum_name) enum_name,
	LIST_OF_LISP_TYPES
#undef X
};

// COMPARISON

enum cmp_result {
	CMP_LT = -1,
	CMP_EQ = 0,
	CMP_GT = 1,
};

// the c types for each lisp type (declaration)

struct value;
typedef struct value lisp_value_t;

typedef i64 lisp_number_t;

struct builtin;
typedef struct builtin *builtin_t;
typedef builtin_t lisp_builtin_t;

struct symbol;
typedef struct symbol *symbol_t;
typedef symbol_t lisp_symbol_t;

struct list;
typedef struct list *list_t;
typedef list_t lisp_list_t;

typedef bool lisp_boolean_t;

struct string;
typedef struct string *string_t;
typedef string_t lisp_string_t;

enum cmp_result value_cmp(const lisp_value_t lhs, const lisp_value_t rhs);
void value_repr(const lisp_value_t this, struct string_builder *sb);
bool value_is_truthy(const lisp_value_t this);
#define X(type_name, _) \
enum cmp_result type_name ## _cmp(const lisp_ ## type_name ## _t lhs, const lisp_ ## type_name ## _t rhs); \
void type_name ## _repr(const lisp_ ## type_name ## _t this, struct string_builder *sb); \
bool type_name ## _is_truthy(const lisp_ ## type_name ## _t this);
LIST_OF_LISP_TYPES
#undef X

void value_increfs(lisp_value_t this);
void value_decrefs(lisp_value_t this);
void value_destroy(lisp_value_t this);
struct value value_copy(struct value this);
#define X(type_name, _) \
void type_name ## _increfs(lisp_ ## type_name ## _t this); \
void type_name ## _decrefs(lisp_ ## type_name ## _t this); \
void type_name ## _destroy(lisp_ ## type_name ## _t this); \
lisp_ ## type_name ## _t type_name ## _copy(lisp_ ## type_name ## _t this);
LIST_OF_GCD_TYPES
#undef X

#define def_trivial_increfs(type_name) \
void type_name ## _increfs(lisp_ ## type_name ## _t this) { \
	assert(this != NULL); \
	assert(this->refcount != 0); \
	++this->refcount; \
}
#define def_trivial_decrefs(type_name) \
void type_name ## _decrefs(lisp_ ## type_name ## _t this) { \
	assert(this != NULL); \
	assert(this->refcount != 0); \
	--this->refcount; \
	if (this->refcount == 0) { \
		type_name ## _destroy(this); \
	} \
}

// LISP VALUES

// + init +

// WARNING: NO INCREASING OF REFCOUNTS ARE DONE IN THESE INITIALISERS!
#define X(type_name, _) \
struct value value_of_ ## type_name(lisp_ ## type_name ## _t type_name);
LIST_OF_LISP_TYPES
#undef X

// (CALL RESULT)

struct call_res;

// BULITINS

// + misc +

struct _builtin_call_opts {
	bool inhibit_argument_evaluation;
};
struct call_res _builtin_call(
	const struct builtin *this,
	const lisp_list_t *args,
	struct _builtin_call_opts opts
);
#define builtin_call(this, args, ...) _builtin_call( \
	&(this), \
	&(args), \
	(struct _builtin_call_opts) { \
		.inhibit_argument_evaluation = false, \
		__VA_ARGS__ \
	} \
)

// SYMBOLS

// + init +

symbol_t symbol_from_strn(const char *str, size_t n);
symbol_t symbol_from_str(const char *str);

// LISTS

// + init +

list_t list_cons(lisp_value_t head, list_t tail);
// WARNING: doesn't increase refcount of tail!
list_t list_cons_with(lisp_value_t head, list_t tail);

// + manipulation +

list_t list_dup(list_t this);
// WARNING: all manipulation functions assume that this->info->net_refcount == 1
// this is checked with an assertion!
list_t list_append(list_t this, lisp_value_t val);
list_t list_reverse(list_t this);

// + misc +

struct _list_call_opts {
	bool is_tail_call;
	bool inhibit_argument_evaluation;
};
struct call_res _list_call(
	const struct list *this,
	const lisp_list_t *args,
	struct _list_call_opts opts
);
#define list_call(this, args, ...) _builtin_call( \
	&(this), \
	&(args), \
	(struct _list_call_opts) { \
		.is_tail_call = false, \
		.inhibit_argument_evaluation = false, \
		__VA_ARGS__ \
	} \
)

// STRINGS

// + init +

string_t string_from_strn(const char *str, size_t len);
string_t string_from_str(const char *str);
string_t string_from_sb(struct string_builder *sb);
string_t string_substr(string_t of, size_t begin, size_t end);
// WARNING: consumes the string builder!
string_t sb_to_string(struct string_builder *sb);

/* SECTION: LISP TYPES AND VALUES (DEFINITION) */

// LISP VALUES

struct value {
	enum lisp_type type;
	union {
#define X(type_name, _) lisp_ ## type_name ## _t type_name;
		LIST_OF_LISP_TYPES
#undef X
	} as;
};

// + required functions +

enum cmp_result value_cmp(const lisp_value_t lhs, const lisp_value_t rhs) {
	assert(lhs.type == rhs.type);

	switch (lhs.type) {
#define X(type_name, enum_name) \
		case enum_name: return type_name ## _cmp(lhs.as.type_name, rhs.as.type_name);
		LIST_OF_LISP_TYPES
#undef X
	}

	assert(false && "unreachable");
}
void value_repr(const lisp_value_t this, struct string_builder *sb) {
	switch (this.type) {
#define X(type_name, enum_name) \
		case enum_name: { type_name ## _repr(this.as.type_name, sb); } break;
		LIST_OF_LISP_TYPES
#undef X
	}

	assert(false && "unreachable");
}
bool value_is_truthy(const lisp_value_t this) {
	switch (this.type) {
#define X(type_name, enum_name) \
		case enum_name: return type_name ## _is_truthy(this.as.type_name);
		LIST_OF_LISP_TYPES
#undef X
	}

	assert(false && "unreachable");
}
void value_increfs(lisp_value_t this) {
	switch (this.type) {
#define X(type_name, enum_name) \
		case enum_name: { type_name ## _increfs(this.as.type_name); } break;
		LIST_OF_GCD_TYPES
#undef X
		default: break;
	}
}
void value_decrefs(lisp_value_t this) {
	switch (this.type) {
#define X(type_name, enum_name) \
		case enum_name: { type_name ## _decrefs(this.as.type_name); } break;
		LIST_OF_GCD_TYPES
#undef X
		default: break;
	}
}
void value_destroy(lisp_value_t this) {
	switch (this.type) {
#define X(type_name, enum_name) \
		case enum_name: type_name ## _destroy(this.as.type_name); break;
		LIST_OF_GCD_TYPES
#undef X
		default: break;
	}
}

// + init +
#define X(type_name, enum_name) \
struct value value_of_ ## type_name(lisp_ ## type_name ## _t type_name) { \
	return (struct value) { \
		.type = enum_name, \
		.as.type_name = type_name, \
	}; \
}
LIST_OF_LISP_TYPES
#undef X

// NUMBERS

// + required functions +

enum cmp_result number_cmp(const lisp_number_t lhs, const lisp_number_t rhs) {
	return lhs < rhs ? CMP_LT : lhs > rhs ? CMP_GT : CMP_EQ;
}
void number_repr(const lisp_number_t this, struct string_builder *sb) {
	u64 uthis = *(u64*)&this;

	if (this < 0) {
		sb_addc(sb, '-');
		uthis = 1+~uthis; // negate using two's complement
		// this is done this way since the negation of -2**63 is not representable in the i64 type
	}
	if (this == 0) {
		sb_addc(sb, '0');
		return;
	}

	// construct the number in least-significant-digit-first order
	const size_t begin = sb->count;
	while (uthis) {
		sb_addc(sb, '0'+(uthis%10));
		uthis /= 10;
	}
	const size_t len = sb->count - begin;

	// reverse the lsdf representation to get most-significant-digit-first
	for (size_t i = 0; i < len/2; ++i) {
		// swap repr[i] and repr[len-1 - i]
		const char tmp = sb->items[begin + i];
		sb->items[begin + i] = sb->items[begin+len-1 - i];
		sb->items[begin+len-1 - 1] = tmp;
	}
}
bool number_is_truthy(const lisp_number_t this) {
	return this != 0;
}

// (CALL RESULT)

struct call_res {
	lisp_value_t result;
	bool destroy_env;
	bool eval;
};
#define call_res_from(value, ...) ((struct call_res) { .result = (value), .destroy_env = false, .eval = false, __VA_ARGS__ })

// BUILTINS

typedef struct call_res (*builtin_function)(lisp_list_t args);

struct builtin {
	builtin_function fn;
	// should arguments be evaluated before being passed to the function?
	bool eval_args;
	symbol_t name;
	const char *doc; // TODO: switch to string_t?
};

// + required functions +

enum cmp_result builtin_cmp(const lisp_builtin_t lhs, const lisp_builtin_t rhs) {
	assert(false && "TODO");
}
void builtin_repr(const lisp_builtin_t this, struct string_builder *sb) {
	if (this->eval_args) {
		sb_adds(sb, "<builtin function ");
		symbol_repr(this->name, sb);
		sb_addc(sb, '>');
	} else {
		sb_adds(sb, "<builtin macro ");
		symbol_repr(this->name, sb);
		sb_addc(sb, '>');
	}
}
bool builtin_is_truthy(const lisp_builtin_t this) {
	return true;
}

// + misc +

struct call_res _builtin_call(
	const struct builtin *this,
	const lisp_list_t *args,
	struct _builtin_call_opts opts
) {
	assert(false && "TODO");
}

// SYMBOLS

struct symbol {
	const char *sym;

	size_t refcount;
};

// + required functions +

enum cmp_result symbol_cmp(const symbol_t lhs, const symbol_t rhs) {
	if (lhs == rhs) return CMP_EQ;
	const int cmpres = strcmp(lhs->sym, rhs->sym);
	return cmpres < 0 ? CMP_LT : cmpres > 0 ? CMP_GT : CMP_EQ;
}
void symbol_repr(const lisp_symbol_t this, struct string_builder *sb) {
	sb_adds(sb, this->sym);
}
bool symbol_is_truthy(const lisp_symbol_t this) {
	return true;
}
def_trivial_increfs(symbol)
def_trivial_decrefs(symbol)
void symbol_destroy(lisp_symbol_t this) {
	assert(this != NULL);

	free((void*)this->sym);
	free(this);
}

// + init +

symbol_t symbol_from_strn(const char *str, size_t n) {
	assert(str != NULL);

	symbol_t res = malloc(sizeof(*res));
	*res = (struct symbol) {
		.sym = strndup(str, n),
		.refcount = 1,
	};
	return res;
}
symbol_t symbol_from_str(const char *str) {
	return symbol_from_strn(str, strlen(str));
}

// LISTS

struct list_info {
	list_t last;
	size_t net_refcount;
};
struct list {
	struct value val;
	list_t next;

	struct list_info *info;
	size_t refcount;
};

const struct value nil = {
	.type = LT_LIST,
	.as.list = NULL,
};

// + required functions +

enum cmp_result list_cmp(const lisp_list_t lhs, const lisp_list_t rhs) {
	assert(false && "TODO");
}
void list_repr(const lisp_list_t this, struct string_builder *sb) {
	sb_addc(sb, '(');

	lisp_list_t curr = this;
	while (curr != NULL) {
		value_repr(curr->val, sb);
		if (curr->next != NULL) sb_addc(sb, ' ');
		curr = curr->next;
	}

	sb_addc(sb, ')');
}
bool list_is_truthy(const lisp_list_t this) {
	return this != NULL;
}
void list_increfs(lisp_list_t this) {
	assert(this != NULL);
	assert(this->refcount != 0);
	++this->refcount;
	assert(this->info->net_refcount != 0);
	++this->info->net_refcount;
}
void list_decrefs(lisp_list_t this) {
	assert(this != NULL);
	assert(this->refcount != 0);
	--this->refcount;
	assert(this->info->net_refcount != 0);
	--this->info->net_refcount;
	if (this->refcount == 0) {
		list_destroy(this);
	}
}
void list_destroy(lisp_list_t this) {
	assert(this->info != NULL);

	if (this->next) list_decrefs(this->next);
	value_decrefs(this->val);
	if (this == this->info->last) {
		assert(this->info->net_refcount == 0);
		free(this->info);
	}
	free(this);
}

// + init +

list_t list_cons(lisp_value_t head, list_t tail) {
	if (tail != NULL) list_copy(tail);
	return list_cons_with(head, tail);
}
list_t list_cons_with(lisp_value_t head, list_t tail) {
	list_t res = malloc(sizeof(*res));
	if (tail == NULL) {
		struct list_info *info = malloc(sizeof(*info));
		*info = (struct list_info) {
			.last = res,
			.net_refcount = 1,
		};
		*res = (struct list) {
			.val = value_copy(head),
			.next = NULL,
			.info = info,
			.refcount = 1,
		};
	} else {
		*res = (struct list) {
			.val = value_copy(head),
			.next = tail,
			.info = tail->info,
			.refcount = 1,
		};
	}
	return res;
}

// + manipulation +

list_t list_dup(list_t this) {
	list_t res = NULL;

	while (this != NULL) {
		res = list_append(res, this->val);

		this = this->next;
	}

	return res;
}
// WARNING: all manipulation functions assume that this->info->net_refcount == 1
// this is checked with an assertion!
list_t list_append(list_t this, lisp_value_t val) {
	if (this == NULL) {
		return list_cons(val, NULL);
	}
	assert(this->info->net_refcount == 0);

	list_t new_last = malloc(sizeof(new_last));
	*new_last = (struct list) {
		.val = value_copy(val),
		.next = NULL,
		.info = this->info,
		.refcount = 1,
	};
	this->info->last->next = new_last;
	this->info->last = new_last;

	return this;
}
list_t list_reverse(list_t this) {
	if (this == NULL) return NULL;
	assert(this->info->net_refcount == 0);

	assert(false && "TODO");
}

// + misc +

struct call_res _list_call(
	const struct list *this,
	const lisp_list_t *args,
	struct _list_call_opts opts
) {
	assert(false && "TODO");
}

// BOOLEANS

// + required functions +

enum cmp_result boolean_cmp(const lisp_boolean_t lhs, const lisp_boolean_t rhs) {
	return lhs < rhs ? CMP_LT : lhs > rhs ? CMP_GT : CMP_EQ;
}
void boolean_repr(const lisp_boolean_t this, struct string_builder *sb) {
	sb_addc(sb, this ? 'T' : 'F');
}
bool boolean_is_truthy(const lisp_boolean_t this) {
	return this;
}

// STRINGS

const char escapes[][2] = {
	{ '\0', '0', },
	{ '\t', 't', },
	{ '\v', 'v', },
	{ '\r', 'r', },
	{ '\n', 'n', },
	{ '\\', '\\', },
	{ '"', '"', },
	{ '\a', 'a', },
	{ '\b', 'b', },
};

struct string {
	char *data;
	size_t len;

	string_t borrows;
	size_t refcount;
};

// + required functions +

enum cmp_result string_cmp(const lisp_string_t lhs, const lisp_string_t rhs) {
	assert(false && "TODO");
}
void string_repr(const lisp_string_t this, struct string_builder *sb) {
	sb_addc(sb, '"');
	for (size_t i = 0; i < this->len; ++i) {
		for (size_t j = 0; j < sizeof(escapes)/sizeof(*escapes); ++j) {
			if (escapes[j][0] == this->data[i]) {
				sb_addc(sb, '\\');
				sb_addc(sb, escapes[j][1]);
				goto escaped;
			}
		}
		sb_addc(sb, this->data[i]);
	escaped:;
	}
	sb_addc(sb, '"');
}
bool string_is_truthy(const lisp_string_t this) {
	return this->len != 0;
}
def_trivial_increfs(string)
def_trivial_decrefs(string)
void string_destroy(lisp_string_t this) {
	if (this->borrows == NULL) free((void*)this->data);
	else string_decrefs(this->borrows);
	free(this);
}

// + init +

string_t string_from_strn(const char *str, size_t len) {
	assert(str != NULL);

	string_t res = malloc(sizeof(*res));
	*res = (struct string) {
		.data = strndup(str, len),
		.len = len,
		.borrows = NULL,
		.refcount = 1,
	};
	return res;
}
string_t string_from_str(const char *str) {
	return string_from_strn(str, strlen(str));
}
string_t string_from_sb(struct string_builder *sb) {
	return string_from_strn(sb->items, sb->count);
}
string_t string_substr(string_t of, size_t begin, size_t end) {
	assert(of != NULL);
	assert(begin <= end);
	assert(end <= of->len);

	string_t res = malloc(sizeof(*res));
	*res = (struct string) {
		.data = &of->data[begin],
		.len = end - begin,
		.borrows = string_copy(of),
		.refcount = 1,
	};
	return res;
}
string_t sb_to_string(struct string_builder *sb) {
	string_t res = malloc(sizeof(*res));
	*res = (struct string) {
		.data = sb->items,
		.len = sb->count,
		.borrows = NULL,
		.refcount = 1,
	};

	sb->items = NULL;
	sb->count = 0;
	sb->capacity = 0;
	sb_clear(sb);

	return res;
}

/* SECTION: GC HELPERS */


lisp_value_t value_copy(lisp_value_t this) {
	value_increfs(this);
	return this;
}
#define X(type_name, _) \
lisp_ ## type_name ## _t type_name ## _copy(lisp_ ## type_name ## _t this) { \
	type_name ## _increfs(this); \
	return this; \
}
LIST_OF_GCD_TYPES
#undef X

/* SECTION: ENVIRONMENTS */

struct assoc {
	lisp_symbol_t name;
	lisp_value_t value;
};
struct env {
	struct env *parent;
	bool fixed;
	lisp_list_t params_of;

	struct assoc *items;
	size_t count;
	size_t capacity;
};

#define env_foreach(env, it) da_foreach(env, struct assoc, it)
void env_def(struct env *env, lisp_symbol_t name, lisp_value_t value);
struct assoc *find_var(struct env *env, const lisp_symbol_t name);
void env_clear(struct env *env);
void env_free(struct env *env);

void env_def(struct env *env, lisp_symbol_t name, lisp_value_t value) {
	da_append(env, ((struct assoc) { symbol_copy(name), value_copy(value), }));
}
struct assoc *find_var(struct env *env, const lisp_symbol_t name) {
	if (env == NULL) return NULL;

	size_t i = env->count;
	while (i --> 0) {
		if (symbol_cmp(env->items[i].name, name) == CMP_EQ) return &env->items[i];
	}

	return find_var(env->parent, name);
}
void env_clear(struct env *env) {
	assert(env != NULL);

	if (env->params_of) {
		list_decrefs(env->params_of);
		env->params_of = NULL;
	}

	env_foreach(env, it) {
		symbol_decrefs(it->name);
		value_decrefs(it->value);
	}

	// if the env is used after being freed,
	// this ensures that the program crashes early
	// rather than trucking on like nothing's wrong
	memset(env->items, 0, sizeof(*env->items)*env->capacity);
	free(env->items);

	env->count = 0;
	env->capacity = 0;
}
void env_free(struct env *env) {
	assert(env != NULL);

	env_clear(env);
	*env = (struct env) {0};
	free(env);
}

/* SECTION: REFACTOR PENDING */

#define advance() do { ++*str; --*str_len; } while (0)

void skip_ws(const char **str, size_t *str_len) {
	while (*str_len && (
		**str == ' ' || **str == '\t' || **str == '\n' || **str == ';'
	)) {
		if (**str == ';') {
			while (*str_len && **str != '\n') {
				advance();
			}
		}
		advance();
	}
}

bool is_break(char c) {
	return c == ' ' || c == '(' || c == ')' || c == '\t' || c == '\n' || c == ';' || c == '"';
}

struct lisp_val quote(struct lisp_val val) {
	static struct lisp_val quote_sym = {
		.type = LT_SYM,
		.as.sym = NULL,
	};
	if (quote_sym.as.sym == NULL) {
		quote_sym.as.sym = sym_from_str("quote");
	}

	list_t lst = NULL;
	lst = list_append(lst, quote_sym);
	lst = list_append(lst, val);
	return (struct lisp_val) {
		.type = LT_LIST,
		.as.list = lst,
	};
}

struct lisp_val lisp_val_parse(const char **str, size_t *str_len);
list_t list_parse(const char **str, size_t *str_len) {
	list_t res = NULL;

	for (;;) {
		skip_ws(str, str_len);
		if (*str_len == 0) {
			fprintf(stderr, "expected value or end of list, got EOF\n");
			exit(1);
		}
		if (**str == ')') {
			advance();
			return res;
		}

		struct lisp_val tmp = lisp_val_parse(str, str_len);
		res = list_append(res, tmp);
		lisp_val_free(tmp);
	}
}

i64 num_parse(const char **str, size_t *str_len) {
	i64 res = 0;

	while (*str_len && '0' <= **str && **str <= '9') {
		res *= 10;
		res += **str - '0';
		advance();
	}

	return res;
}

sym_t sym_parse(const char **str, size_t *str_len) {
	const char *begin = *str;

	while (*str_len && !is_break(**str)) {
		advance();
	}

	return sym_from_strn(begin, (*str)-begin);
}

string_t string_parse(const char **str, size_t *str_len) {
	assert(**str == '"');
	advance();
	const char *begin = *str;

	while (*str_len && **str != '"') {
		if (**str == '\\') advance();
		if (*str_len) advance();
	}

	if (*str_len == 0) {
		fprintf(stderr, "unexpected EOF while parsing string\n");
		exit(1);
	}

	assert(**str == '"');
	const char *end = *str;
	advance();

	string_t res = malloc(sizeof(*res));
	*res = (struct string) {
		.data = malloc(sizeof(char)*(end-begin)),
		.len = 0,

		.borrows = NULL,
		.refcount = 1,
	};

	for (const char *c = begin; c < end; ++c) {
		if (*c == '\\') {
			++c;
			for (size_t i = 0; i < sizeof(escapes)/sizeof(*escapes); ++i) {
				if (escapes[i][1] == *c) {
					res->data[res->len++] = escapes[i][0];
					goto escaped;	
				}
			}
			fprintf(stderr, "while parsing string, found \\%c.\n", *c);
			assert(false && "unrecognised escape code");
	escaped:;
		} else {
			res->data[res->len++] = *c;
		}
	}

	return res;
}


i64 char_parse(const char **str, size_t *str_len) {
	skip_ws(str, str_len);

	if (*str_len == 0) {
		fprintf(stderr, "unexpected EOF while parsing character\n");
		exit(1);
	}

	const char c = **str;
	advance();

	if (c == '\\') {
		if (*str_len == 0) {
			fprintf(stderr, "unexpected EOF while parsing character\n");
			exit(1);
		}

		const char e = **str;
		advance();

		for (size_t i = 0; i < sizeof(escapes)/sizeof(*escapes); ++i) {
			if (escapes[i][1] == e) {
				return escapes[i][0];
			}
		}
		assert(false && "unrecognised escape code");
	}

	return c;
}

struct lisp_val lisp_val_parse(const char **str, size_t *str_len) {
	skip_ws(str, str_len);
	if (*str_len == 0) {
		fprintf(stderr, "expected value, got EOF\n");
		exit(1);
	}
	if (**str == '(') {
		advance();
		return (struct lisp_val) {
			.type = LT_LIST,
			.as.list = list_parse(str, str_len),
		};
	} else if (**str == '"') {
		return (struct lisp_val) {
			.type = LT_STRING,
			.as.string = string_parse(str, str_len),
		};
	} else if ('0' <= **str && **str <= '9') {
		return (struct lisp_val) {
			.type = LT_NUM,
			.as.num = num_parse(str, str_len),
		};
	} else if ((**str == 'T' || **str == 'F') && (*str_len == 1 || is_break(*((*str)+1)))) {
		const char c = **str;
		advance();
		return (struct lisp_val) {
			.type = LT_BOOL,
			.as.boo = c == 'T',
		};
	} else if (**str == '\'' && (*str_len == 1 || is_break(*((*str)+1)))) {
		advance();
		struct lisp_val tmp = lisp_val_parse(str, str_len);
		struct lisp_val res = quote(tmp);
		lisp_val_free(tmp);
		return res;
	} else if (**str == '#' && (*str_len == 1 || is_break(*((*str)+1)))) {
		advance();
		return (struct lisp_val) {
			.type = LT_NUM,
			.as.num = char_parse(str, str_len),
		};
	} else {
		return (struct lisp_val) {
			.type = LT_SYM,
			.as.sym = sym_parse(str, str_len),
		};
	}
}

struct sym _nil_sym = { .sym = "nil", .refcount = 1 };
struct env root_env = {0};
struct env *curr_env = &root_env;

struct call_res ladd(list_t args) {
	assert(args != NULL);

	i64 res = 0;

	while (args != NULL) {
		assert(args->val.type == LT_NUM);
		res += args->val.as.num;

		args = args->next;
	}

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = res,
	}));
}
struct call_res lsub(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUM);

	if (args->next == NULL) {
		lret(((struct lisp_val) {
			.type = LT_NUM,
			.as.num = -args->val.as.num,
		}));
	}

	i64 res = args->val.as.num;
	args = args->next;

	while (args != NULL) {
		assert(args->val.type == LT_NUM);
		res -= args->val.as.num;

		args = args->next;
	}

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = res,
	}));
}
struct call_res llist_fn(list_t args) {
	lret(((struct lisp_val) {
		.type = LT_LIST,
		.as.list = list_copy(args),
	}));
}
struct call_res lcons(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);
	assert(args->next->next == NULL);
	assert(args->next->val.type == LT_LIST);

	lret(((struct lisp_val) {
		.type = LT_LIST,
		.as.list = list_cons(args->val, args->next->val.as.list),
	}));
}
struct call_res lappend(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_LIST,
		.as.list = list_append(list_dup(args->val.as.list), args->next->val),
	}));
}
struct call_res lquote(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	lret(lisp_val_copy(args->val));
}
struct lisp_val eval(struct lisp_val val);
struct call_res leval(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	lret(lisp_val_copy(args->val), .eval = true);
}
struct call_res ldef(list_t args) {
	while (args != NULL) {
		assert(args->val.type == LT_SYM);
		assert(args->next != NULL);

		struct env *env = curr_env;
		while (env->fixed) {
			assert(env->parent != NULL);
			env = env->parent;
		}
		struct lisp_val tmp = eval(args->next->val);
		env_def(env, args->val.as.sym, tmp);
		lisp_val_free(tmp);

		args = args->next->next;
	}

	lret(lisp_val_copy(nil));
}
struct call_res lassoc(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next != NULL);
	assert(args->next->next == NULL);

	struct env *env = malloc(sizeof(struct env));
	*env = (struct env) {0};
	env->parent = curr_env;
	env->fixed = true;
	curr_env = env;

	list_t vars = args->val.as.list;
	while (vars != NULL) {
		assert(vars->val.type == LT_SYM);
		assert(vars->next != NULL);

		struct lisp_val tmp = eval(vars->next->val);
		env_def(curr_env, vars->val.as.sym, tmp);
		lisp_val_free(tmp);

		vars = vars->next->next;
	}

	lret(lisp_val_copy(args->next->val), .eval = true, .destroy_env = true);
}
struct call_res lenv_new(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	struct env *env = malloc(sizeof(struct env));
	*env = (struct env) {0};
	env->parent = curr_env;
	curr_env = env;

	lret(lisp_val_copy(args->val), .eval = true, .destroy_env = true);
}
struct call_res lset(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_SYM);
	assert(args->next != NULL);
	assert(args->next->next == NULL);

	struct assoc *assoc = find_var(curr_env, args->val.as.sym);
	assert(assoc != NULL);

	// freeing assoc->val immediately caused me *so* much pain when I tried to set a string to its own substring...
	struct lisp_val tmp = assoc->value;
	assoc->value = eval(args->next->val);
	lisp_val_free(tmp);

	lret(lisp_val_copy(assoc->value));
}
struct call_res ltruthy(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_BOOL,
		.as.boo = lisp_val_is_truthy(args->val),
	}));
}
struct call_res lif(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);
	assert(args->next->next == NULL || args->next->next->next == NULL);

	struct lisp_val tmp = eval(args->val);
	bool cond = lisp_val_is_truthy(tmp);
	lisp_val_free(tmp);

	if (cond) {
		lret(lisp_val_copy(args->next->val), .eval = true);
	} else if (args->next->next != NULL) {
		lret(lisp_val_copy(args->next->next->val), .eval = true);
	} else {
		lret(lisp_val_copy(nil));
	}
}
struct call_res ldo(list_t args) {
	if (args == NULL) lret(lisp_val_copy(nil));

	while (args->next != NULL) {
		lisp_val_free(eval(args->val));
		args = args->next;
	}

	lret(lisp_val_copy(args->val), .eval = true);
}

struct list_fn {
	list_t params;
	bool is_macro;
	sym_t name;
	string_t doc;
	struct lisp_val body;
};
struct list_fn list_fn_copy(struct list_fn this) {
	list_copy(this.params);
	if (this.name != NULL) sym_copy(this.name);
	if (this.doc != NULL) string_copy(this.doc);
	lisp_val_copy(this.body);

	return this;
}
void list_fn_free(struct list_fn this) {
	list_free(this.params);
	if (this.name != NULL) sym_free(this.name);
	if (this.doc != NULL) string_free(this.doc);
	lisp_val_free(this.body);
}
bool list_is_fn(list_t list) {
	if (list == NULL) return false;
	if (list->val.type != LT_LIST) return false;

	list_t params = list->val.as.list;
	while (params != NULL) {
		if (params->val.type == LT_SYM) {
			params = params->next;
			continue;
		}

		if (params->val.type != LT_LIST) return false;
		if (params->next == NULL) return false;
		if (params->next->val.type != LT_SYM) return false;
		if (params->next->next != NULL) return false;
		break;
	}

	if (list->next == NULL) return false;
	if (list->next->val.type == LT_LIST) {
		if (list->next->next == NULL) return false;
		if (list->next->next->val.type != LT_BOOL) return false;
		if (list->next->next->next == NULL) return false;
		if (list->next->next->next->next != NULL) return false;

		list_t meta = list->next->val.as.list;

		if (meta == NULL) return false;
		if (meta->val.type != LT_SYM) return false;
		if (meta->next == NULL) return false;
		if (meta->next->val.type != LT_STRING) return false;
		if (meta->next->next != NULL) return false;

		return true;
	} else if (list->next->val.type == LT_BOOL) {
		if (list->next->next == NULL) return false;
		if (list->next->next->next != NULL) return false;
		return true;
	} else return false;
}
struct list_fn list_to_fn(list_t list) {
	assert(list != NULL);

	assert(list_is_fn(list));

	struct list_fn res;

	if (list->next->val.type == LT_LIST) res = (struct list_fn) {
		.params = list_copy(list->val.as.list),
		.name = sym_copy(list->next->val.as.list->val.as.sym),
		.doc = string_copy(list->next->val.as.list->next->val.as.string),
		.is_macro = list->next->next->val.as.boo,
		.body = lisp_val_copy(list->next->next->next->val),
	}; else res = (struct list_fn) {
		.params = list_copy(list->val.as.list),
		.name = NULL,
		.doc = NULL,
		.is_macro = list->next->val.as.boo,
		.body = lisp_val_copy(list->next->next->val),
	};

	return res;
}

struct call_res lcall(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_BUILTIN || args->val.type == LT_LIST);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_LIST);
	assert(args->next->next == NULL);

	if (args->val.type == LT_BUILTIN) {
		return lb_call(args->val.as.builtin, args->next->val.as.list, true);
	} else {
		return ll_call(args->val.as.list, args->next->val.as.list, false, true);
	}
}
struct call_res lpstr(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING || args->val.type == LT_NUM);
	assert(args->next == NULL);

	if (args->val.type == LT_STRING) {
		printf("%.*s", (int)args->val.as.string->len, args->val.as.string->data);
	} else {
		assert(0 <= args->val.as.num && args->val.as.num < 256);

		putchar(args->val.as.num);
	}

	lret(nil);
}
struct call_res lhead(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next == NULL);

	assert(args->val.as.list != NULL);
	lret(lisp_val_copy(args->val.as.list->val));
}
struct call_res ltail(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next == NULL);

	assert(args->val.as.list != NULL);
	lret(((struct lisp_val) {
		.type = LT_LIST,
		.as.list = list_copy(args->val.as.list->next),
	}));
}
struct call_res lnth(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUM);
	assert(args->next->next == NULL);

	list_t lst = args->val.as.list;
	for (i64 i = 0; i < args->next->val.as.num; ++i) {
		assert(lst != NULL);
		lst = lst->next;
	}

	assert(lst != NULL);
	lret(lisp_val_copy(lst->val));
}
struct lisp_val substitute(struct lisp_val into, struct env *from);
struct call_res lsubs(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	lret(substitute(args->val, curr_env));
}
struct call_res lsubs_with(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next != NULL);
	assert(args->next->next == NULL);

	struct env env = {0};

	list_t subs = args->val.as.list;

	while (subs != NULL) {
		assert(subs->val.type == LT_SYM);
		assert(subs->next != NULL);

		struct lisp_val tmp = eval(subs->next->val);
		env_def(&env, subs->val.as.sym, tmp);
		lisp_val_free(tmp);

		subs = subs->next->next;
	}

	struct lisp_val tmp = eval(args->next->val);
	struct lisp_val res = substitute(tmp, &env);
	lisp_val_free(tmp);

	env_clear(&env);

	lret(res);
}

struct call_res l_div(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUM);
	assert(args->next != NULL);

	i64 res = args->val.as.num;
	args = args->next;

	while (args != NULL) {
		assert(args->val.type == LT_NUM);
		res /= args->val.as.num;

		args = args->next;
	}

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = res,
	}));
}

struct call_res lmod(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUM);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUM);
	assert(args->next->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = args->val.as.num % args->next->val.as.num,
	}));
}

i64 sign(i64 n) {
	return n < 0 ? -1 : n == 0 ? 0 : 1;
}
i64 lisp_val_cmp(struct lisp_val a, struct lisp_val b) {
	assert(a.type == b.type);

	switch (a.type) {
		case LT_NUM: return a.as.num < b.as.num ? -1 : a.as.num == b.as.num ? 0 : 1;
		case LT_SYM: return sign(strcmp(a.as.sym->sym, b.as.sym->sym));
		case LT_BOOL: return a.as.boo < b.as.boo ? -1 : a.as.boo == b.as.boo ? 0 : 1;
		case LT_BUILTIN: assert(false && "TODO");
		case LT_LIST: {
			list_t la = a.as.list;
			list_t lb = b.as.list;

			while (la != NULL && lb != NULL) {
				const i64 cmp = lisp_val_cmp(la->val, lb->val);
				if (cmp) return cmp;

				la = la->next;
				lb = lb->next;
			}

			return la == NULL && lb == NULL ? 0 : la == NULL ? -1 : 1;
		}
		case LT_STRING: {
			string_t sa = a.as.string;
			string_t sb = b.as.string;

			if (sa == sb) return 0;

			const size_t upto = sa->len < sb->len ? sa->len : sb->len;
			for (size_t i = 0; i < upto; ++i) {
				if (sa->data[i] < sb->data[i]) return -1;
				else if (sa->data[i] > sb->data[i]) return 1;
			}
			return sa->len == sb->len ? 0 : sa->len < sb->len ? -1 : 1;
		}
	}

	assert(false && "unreachable");
}
struct call_res lcmp(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);
	assert(args->next->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = lisp_val_cmp(args->val, args->next->val),
	}));
}

struct call_res llsh(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUM);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUM);
	assert(args->next->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = args->val.as.num << args->next->val.as.num,
	}));
}

struct call_res lrsh(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUM);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUM);
	assert(args->next->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = args->val.as.num >> args->next->val.as.num,
	}));
}

struct call_res lbnot(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUM);
	assert(args->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = ~args->val.as.num,
	}));
}

struct call_res lband(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUM);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUM);
	assert(args->next->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = args->val.as.num & args->next->val.as.num,
	}));
}

struct call_res lbor(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUM);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUM);
	assert(args->next->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = args->val.as.num | args->next->val.as.num,
	}));
}

struct call_res lbxor(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUM);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUM);
	assert(args->next->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = args->val.as.num ^ args->next->val.as.num,
	}));
}

struct call_res land(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);

	while (args->next != NULL) {
		struct lisp_val res = eval(args->val);
		if (!lisp_val_is_truthy(res)) lret(res);
		lisp_val_free(res);
		args = args->next;
	}

	lret(lisp_val_copy(args->val), .eval = true);
}

struct call_res lor(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);

	while (args->next != NULL) {
		struct lisp_val res = eval(args->val);
		if (lisp_val_is_truthy(res)) lret(res);
		lisp_val_free(res);
		args = args->next;
	}

	lret(lisp_val_copy(args->val), .eval = true);
}

struct call_res ltype(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

#define symbol_of(name) \
		static struct lisp_val name##_sym = { .type = LT_SYM, .as.sym = NULL, }; \
		if (name##_sym.as.sym == NULL) name##_sym.as.sym = sym_from_str(#name)

	symbol_of(number);
	symbol_of(symbol);
	symbol_of(builtin);
	symbol_of(list);
	symbol_of(boolean);
	symbol_of(string);

#undef symbol_of

	switch (args->val.type) {
		case LT_NUM: lret(lisp_val_copy(number_sym));
		case LT_SYM: lret(lisp_val_copy(symbol_sym));
		case LT_BUILTIN: lret(lisp_val_copy(builtin_sym));
		case LT_LIST: lret(lisp_val_copy(list_sym));
		case LT_BOOL: lret(lisp_val_copy(boolean_sym));
		case LT_STRING: lret(lisp_val_copy(string_sym));
	}

	assert(false && "unreachable");
}

struct call_res lexit(list_t args) {
	assert(args == NULL || args->next == NULL);

	if (args != NULL) {
		assert(args->val.type == LT_NUM);

		// you can get valgrind to shut up about that 56 bytes of leaked memory when you call exit by uncommenting the following line:
		//list_free(args);
		// however, that breaks the gc model I have (it's not lexit's responsibility to free its args), also I don't know why valgrind thinks it's lost, since it's still somewhere in the callstack
		exit(args->val.as.num);
	} else exit(0);
}
struct call_res ljoin_s(list_t args) {
	assert(args != NULL);

	size_t strlen = 0;
	for (list_t args_it = args; args_it != NULL; args_it = args_it->next) {
		assert(args_it->val.type == LT_STRING || args_it->val.type == LT_NUM);
		assert(args_it->val.type != LT_NUM || (0 <= args_it->val.as.num && args_it->val.as.num < 256));

		if (args_it->val.type == LT_STRING) {
			strlen += args_it->val.as.string->len;
		} else ++strlen;
	}

	string_t res = malloc(sizeof(*res));
	*res = (struct string) {
		.data = malloc(sizeof(char)*strlen),
		.len = 0,

		.borrows = NULL,
		.refcount = 1,
	};

	while (args != NULL) {
		if (args->val.type == LT_STRING) {
			memcpy(&res->data[res->len], args->val.as.string->data, args->val.as.string->len);
			res->len += args->val.as.string->len;
		} else {
			res->data[res->len++] = args->val.as.num;
		}

		args = args->next;
	}
	assert(res->len == strlen);

	lret(((struct lisp_val) {
		.type = LT_STRING,
		.as.string = res,
	}));
}
struct call_res lsubstr_s(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUM);
	assert(args->next->next == NULL || args->next->next->val.type == LT_NUM);
	assert(args->next->next == NULL || args->next->next->next == NULL);

	if (args->next->next == NULL) {
		assert(0 <= args->next->val.as.num && args->next->val.as.num < (i64)args->val.as.string->len);

		lret(((struct lisp_val) {
			.type = LT_NUM,
			.as.num = args->val.as.string->data[args->next->val.as.num],
		}));
	} else {
		string_t s = args->val.as.string;
		const i64 begin = args->next->val.as.num;
		const i64 end = args->next->next->val.as.num;

		assert(begin <= end);
		assert(0 <= begin);
		assert(end <= (i64)s->len);

		lret(((struct lisp_val) {
			.type = LT_STRING,
			.as.string = string_substr(s, begin, end),
		}));
	}
}
struct call_res lrefs(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	struct lisp_val res = {
		.type = LT_NUM,
		.as.num = 0,
	};

	if (args->val.type == LT_SYM)  {
		res.as.num = args->val.as.sym->refcount;
		lret(res);
	} else if (args->val.type == LT_LIST && args->val.as.list != NULL) {
		res.as.num = args->val.as.list->refcount;
		lret(res);
	} else if (args->val.type == LT_STRING) {
		res.as.num = args->val.as.string->refcount;
		lret(res);
	}

	lret(nil);
}
struct call_res lid(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	struct lisp_val res = {
		.type = LT_NUM,
		.as.num = 0,
	};

	if (args->val.type == LT_SYM)  {
		res.as.num = *(i64*)&args->val.as.sym;
		lret(res);
	} else if (args->val.type == LT_LIST) {
		res.as.num = *(i64*)&args->val.as.list;
		lret(res);
	} else if (args->val.type == LT_STRING) {
		res.as.num = *(i64*)&args->val.as.string;
		lret(res);
	} else if (args->val.type == LT_BUILTIN) {
		res.as.num = *(i64*)&args->val.as.builtin.fn;
		lret(res);
	}

	lret(nil);
}

struct call_res lrepr(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	struct string_builder sb = {0};
	lisp_val_repr(args->val, &sb);

	lret(((struct lisp_val) {
		.type = LT_STRING,
		.as.string = sb_to_string(&sb),
	}));
}

struct call_res lparse(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING);
	assert(args->next == NULL);

	list_t res = NULL;

	const char *str = args->val.as.string->data;
	size_t len = args->val.as.string->len;

	skip_ws(&str, &len);
	if (len == 0) {
		string_t tmp = string_from_str("");
		res = list_append(res, (struct lisp_val) {
			.type = LT_STRING,
			.as.string = tmp,
		});
		string_free(tmp);
		lret(((struct lisp_val) {
			.type = LT_LIST,
			.as.list = res,
		}));
	}

	struct lisp_val parsed = lisp_val_parse(&str, &len);
	string_t tmp = string_substr(args->val.as.string, str - args->val.as.string->data, args->val.as.string->len);
	res = list_append(res, (struct lisp_val) {
		.type = LT_STRING,
		.as.string = tmp,
	});
	string_free(tmp);

	res = list_append(res, parsed);
	lisp_val_free(parsed);

	lret(((struct lisp_val) {
		.type = LT_LIST,
		.as.list = res,
	}));
}

struct lisp_val file_to_lisp_val(FILE *f) {
	static struct lisp_val file_sym = {
		.type = LT_SYM,
		.as.sym = NULL,
	};
	if (file_sym.as.sym == NULL) {
		file_sym.as.sym = sym_from_str("file");
	}

	list_t res = NULL;
	res = list_append(res, file_sym);
	res = list_append(res, (struct lisp_val) {
		.type = LT_NUM,
		.as.num = *(i64*)&f,
	});

	return (struct lisp_val) {
		.type = LT_LIST,
		.as.list = res,
	};
}
FILE *lisp_val_to_file(struct lisp_val v) {
	assert(v.type == LT_LIST);
	assert(v.as.list != NULL);
	assert(v.as.list->val.type == LT_SYM);
	assert(strcmp(v.as.list->val.as.sym->sym, "file") == 0);
	assert(v.as.list->next != NULL);
	assert(v.as.list->next->val.type == LT_NUM);
	assert(v.as.list->next->next == NULL);

	return *(FILE**)&v.as.list->next->val.as.num;
}

struct call_res lopen(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING || args->val.type == LT_SYM);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_STRING || args->next->val.type == LT_SYM);
	assert(args->next->next == NULL);

	const char *fname = args->val.type == LT_STRING
		? strndup(args->val.as.string->data, args->val.as.string->len)
		: args->val.as.sym->sym
	;
	const char *fmode = args->next->val.type == LT_STRING
		? strndup(args->next->val.as.string->data, args->next->val.as.string->len)
		: args->next->val.as.sym->sym
	;

	errno = 0;
	FILE *res = fopen(fname, fmode);
	if (errno || !res) {
		fprintf(stderr, "error while processing file %s: ", fname);
		perror("while opening file");
		exit(1);
	}
	if (args->val.type == LT_STRING) free((void*)fname);
	if (args->next->val.type == LT_STRING) free((void*)fmode);

	lret(file_to_lisp_val(res));
}

struct call_res lclose(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next == NULL);

	FILE *f = lisp_val_to_file(args->val);
	fclose(f);

	lret(nil);
}

struct call_res lreadline(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next == NULL);

	FILE *f = lisp_val_to_file(args->val);

	struct string_builder sb = {0};

	enum input_status is = input(&sb, f);

	if (is == IS_EOF && sb.count == 0) {
		lret(nil);
	}

	lret(((struct lisp_val) {
		.type = LT_STRING,
		.as.string = sb_to_string(&sb),
	}));
}

struct call_res lread(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next == NULL);

	FILE *f = lisp_val_to_file(args->val);

	struct string_builder sb = {0};

	char buf[1024];

	for (;;) {
		size_t amt = fread(buf, 1, 1024, f);

		sb_addb(&sb, buf, amt);

		if (amt < 1024) {
			if (ferror(f)) {
				fprintf(stderr, "error while processing file %s: ", "");
				perror("while reading file");
				exit(1);
			} else break;
		}
	}

	lret(((struct lisp_val) {
		.type = LT_STRING,
		.as.string = sb_to_string(&sb),
	}));
}

struct call_res lwrite(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_STRING || args->next->val.type == LT_NUM);
	assert(args->next->next == NULL);

	FILE *f = lisp_val_to_file(args->val);

	if (args->next->val.type == LT_NUM) {
		assert(0 <= args->next->val.as.num && args->next->val.as.num < 256);

		fputc(args->next->val.as.num, f);
	} else {
		const size_t written = fwrite(args->next->val.as.string->data, 1, args->next->val.as.string->len, f);
		assert(written == args->next->val.as.string->len);
	}

	lret(nil);
}

struct call_res lname(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST || args->val.type == LT_BUILTIN);
	assert(args->next == NULL);

	if (args->val.type == LT_LIST) {
		list_t lfn = args->val.as.list;
		assert(list_is_fn(lfn));

		struct list_fn fn = list_to_fn(lfn);

		if (fn.name == NULL) lret(nil);
		else {
			sym_t name = sym_copy(fn.name);
			list_fn_free(fn);
			lret(((struct lisp_val) {
				.type = LT_SYM,
				.as.sym = name,
			}));
		}
	} else {
		struct lbuiltin fn = args->val.as.builtin;

		if (fn.name != NULL) {
			lret(((struct lisp_val) {
				.type = LT_SYM,
				.as.sym = sym_copy(fn.name),
			}));
		} else lret(nil);
	}
}

struct call_res ldocs(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST || args->val.type == LT_BUILTIN);
	assert(args->next == NULL);

	if (args->val.type == LT_LIST) {
		list_t lfn = args->val.as.list;
		assert(list_is_fn(lfn));

		struct list_fn fn = list_to_fn(lfn);

		if (fn.name == NULL) lret(nil);
		else {
			string_t doc = string_copy(fn.doc);
			list_fn_free(fn);
			lret(((struct lisp_val) {
				.type = LT_STRING,
				.as.string = doc,
			}));
		}
	} else {
		struct lbuiltin fn = args->val.as.builtin;

		if (fn.doc != NULL) {
			lret(((struct lisp_val) {
				.type = LT_STRING,
				.as.string = string_from_str(fn.doc),
			}));
		} else lret(nil);
	}
}

struct call_res lis_macro(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST || args->val.type == LT_BUILTIN);
	assert(args->next == NULL);

	if (args->val.type == LT_LIST) {
		list_t lfn = args->val.as.list;
		assert(list_is_fn(lfn));

		struct list_fn fn = list_to_fn(lfn);

		lret(((struct lisp_val) {
			.type = LT_BOOL,
			.as.boo = fn.is_macro,
		}));
	} else {
		struct lbuiltin fn = args->val.as.builtin;

		lret(((struct lisp_val) {
			.type = LT_BOOL,
			.as.boo = !fn.eval_args,
		}));
	}
}

struct call_res lis_callable(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_BOOL,
		.as.boo = args->val.type == LT_BUILTIN || (args->val.type == LT_LIST && list_is_fn(args->val.as.list)),
	}));
}

struct call_res llen_s(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING);
	assert(args->next == NULL);

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = args->val.as.string->len,
	}));
}

struct call_res lffi_load(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING);
	assert(args->next == NULL);

	char *libname = strndup(args->val.as.string->data, args->val.as.string->len);
	void *lib = dlopen(libname, RTLD_LAZY | RTLD_GLOBAL);
	free(libname);

	if (lib == NULL) {
		lret(((struct lisp_val) {
			.type = LT_STRING,
			.as.string = string_from_str(dlerror()),
		}));
	} else {
		list_t res = NULL;
		res = list_cons((struct lisp_val) {
			.type = LT_NUM,
			.as.num = *(i64*)&lib,
		}, res);
		res = list_cons((struct lisp_val) {
			.type = LT_SYM,
			.as.sym = sym_from_str("clib"),
		}, res);
		lret(((struct lisp_val) {
			.type = LT_LIST,
			.as.list = res,
		}));
	}
}

struct call_res lffi_unload(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->val.as.list != NULL);
	assert(args->val.as.list->val.type == LT_SYM);
	assert(strcmp(args->val.as.list->val.as.sym->sym, "clib") == 0);
	assert(args->val.as.list->next != NULL);
	assert(args->val.as.list->next->val.type == LT_NUM);
	assert(args->val.as.list->next->next == NULL);
	assert(args->next == NULL);

	dlclose(*(void**)&args->val.as.list->next->val.as.num);

	lret(nil);
}

struct call_res lffi_sym(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->val.as.list != NULL);
	assert(args->val.as.list->val.type == LT_SYM);
	assert(strcmp(args->val.as.list->val.as.sym->sym, "clib") == 0);
	assert(args->val.as.list->next != NULL);
	assert(args->val.as.list->next->val.type == LT_NUM);
	assert(args->val.as.list->next->next == NULL);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_STRING);
	assert(args->next->next == NULL);

	dlerror();
	char *symname = strndup(args->next->val.as.string->data, args->next->val.as.string->len);
	void *sym = dlsym(*(void**)&args->val.as.list->next->val.as.num, symname);
	free(symname);
	const char *err = NULL;
	if (sym == NULL && (err = dlerror()) != NULL) {
		lret(((struct lisp_val) {
			.type = LT_STRING,
			.as.string = string_from_str(err),
		}));
	} else {
		list_t res = NULL;
		res = list_cons((struct lisp_val) {
			.type = LT_NUM,
			.as.num = *(i64*)&sym,
		}, res);
		res = list_cons((struct lisp_val) {
			.type = LT_SYM,
			.as.sym = sym_from_str("csym"),
		}, res);
		lret(((struct lisp_val) {
			.type = LT_LIST,
			.as.list = res,
		}));
	}

	lret(nil);
}

enum ctype_basic {
	CT_I8,
	CT_U8,
	CT_I32,
	CT_U32,
	CT_I64,
	CT_U64,
	CT_F32,
	CT_F64,
	CT_PTR,
	CT_VOID,
	CT_STRUCT,
};

size_t ctype_size[CT_STRUCT+1] = {
	[CT_I8] = 1,
	[CT_U8] = 1,
	[CT_I32] = 4,
	[CT_U32] = 4,
	[CT_I64] = 8,
	[CT_U64] = 8,
	[CT_F32] = 4,
	[CT_F64] = 8,
	[CT_PTR] = 8,
	[CT_VOID] = 0,
	[CT_STRUCT] = -1,
};

struct ctype {
	enum ctype_basic basic_type;
	union {
		// WARNING: list is not copied!
		list_t struct_members;
	} inner_desc;
};

struct ctype parse_ctype(struct lisp_val val) {
	if (val.type != LT_SYM && val.type != LT_LIST) {
		fputs("C types should either be a symbol, nil, or a list of C types!\n", stderr);
		exit(1);
	}

	if (val.type == LT_SYM) {
		if (strcmp(val.as.sym->sym, "i8") == 0) {
			return (struct ctype) { .basic_type = CT_I8 };
		} else if (strcmp(val.as.sym->sym, "u8") == 0) {
			return (struct ctype) { .basic_type = CT_U8 };
		} else if (strcmp(val.as.sym->sym, "i32") == 0) {
			return (struct ctype) { .basic_type = CT_I32 };
		} else if (strcmp(val.as.sym->sym, "u32") == 0) {
			return (struct ctype) { .basic_type = CT_U32 };
		} else if (strcmp(val.as.sym->sym, "i64") == 0) {
			return (struct ctype) { .basic_type = CT_I64 };
		} else if (strcmp(val.as.sym->sym, "u64") == 0) {
			return (struct ctype) { .basic_type = CT_U64 };
		} else if (strcmp(val.as.sym->sym, "f32") == 0) {
			return (struct ctype) { .basic_type = CT_F32 };
		} else if (strcmp(val.as.sym->sym, "f64") == 0) {
			return (struct ctype) { .basic_type = CT_F64 };
		} else if (strcmp(val.as.sym->sym, "ptr") == 0) {
			return (struct ctype) { .basic_type = CT_PTR };
		} else {
			fprintf(stderr, "Unknown C type `%s`\n", val.as.sym->sym);
			exit(1);
		}
	} else {
		// val.type == LT_LIST

		if (val.as.list == NULL) {
			return (struct ctype) { .basic_type = CT_VOID };
		}

		return (struct ctype) { .basic_type = CT_STRUCT, .inner_desc.struct_members = val.as.list, };
	}
}

size_t get_struct_size(list_t struct_members) {
	size_t res = 0;

	while (struct_members) {
		struct ctype type = parse_ctype(struct_members->val);

		if (type.basic_type == CT_VOID) {
			fputs("Void type should never be a struct member!\n", stderr);
			exit(1);
		}

		if (type.basic_type == CT_STRUCT) {
			if (res % 8) res += res % 8; // alignment
			res += get_struct_size(type.inner_desc.struct_members);
		} else {
			const size_t size = ctype_size[type.basic_type];
			if (res % size) res += res % size; // alignment
			res += size;
		}

		struct_members = struct_members->next;
	}

	return res;
}

size_t construct_cval_into(struct ctype type, struct lisp_val from, void *memory) {
	switch (type.basic_type) {
		case CT_I8: {
			assert(from.type == LT_NUM);
			assert(-(1<<7) <= from.as.num && from.as.num < (1<<7));

			*(int8_t*)memory = from.as.num;
			return ctype_size[CT_I8];
		} break;
		case CT_U8: {
			assert(from.type == LT_NUM);
			assert(0 <= from.as.num && from.as.num < (1<<8));

			*(uint8_t*)memory = from.as.num;
			return ctype_size[CT_U8];
		} break;
		case CT_I32: {
			assert(from.type == LT_NUM);
			assert(-(1l<<31) <= from.as.num && from.as.num < (1l<<31));

			*(int32_t*)memory = from.as.num;
			return ctype_size[CT_I32];
		} break;
		case CT_U32: {
			assert(from.type == LT_NUM);
			assert(0 <= from.as.num && from.as.num < (1l<<32));

			*(uint32_t*)memory = from.as.num;
			return ctype_size[CT_U32];
		} break;
		case CT_I64: {
			assert(from.type == LT_NUM);

			*(i64*)memory = from.as.num;
			return ctype_size[CT_I64];
		} break;
		case CT_U64: {
			assert(from.type == LT_NUM);

			*(u64*)memory = *(u64*)&from.as.num;
			return ctype_size[CT_U64];
		} break;
		case CT_F32: {
			// TODO:
			fputs("Handling of C floats not yet implemented\n", stderr);
			exit(1);
		} break;
		case CT_F64: {
			// TODO:
			fputs("Handling of C floats not yet implemented\n", stderr);
			exit(1);
		} break;
		case CT_PTR: {
			assert(from.type == LT_LIST);
			assert(from.as.list != NULL);
			assert(from.as.list->val.type == LT_SYM);
			assert(strcmp(from.as.list->val.as.sym->sym, "pointer") == 0);
			assert(from.as.list->next->val.type == LT_NUM);
			assert(from.as.list->next->next == NULL);

			*(void**)memory = *(void**)&from.as.list->next->val.as.num;
			return ctype_size[CT_PTR];
		} break;
		case CT_VOID: {
			fputs("Void type cannot be constructed in memory!\n", stderr);
			exit(1);
		} break;
		case CT_STRUCT: {
			assert(from.type == LT_LIST);

			list_t struct_member_types = type.inner_desc.struct_members;
			list_t struct_member_values = from.as.list;

			size_t size_so_far = 0;

			//const void *const orig_mem = memory;

			while (struct_member_types != NULL) {
				assert(struct_member_values != NULL);

				struct ctype member_type = parse_ctype(struct_member_types->val);

				if (member_type.basic_type == CT_VOID) {
					fputs("Void type should never be a struct member!\n", stderr);
					exit(1);
				}

				// alignment
				if (member_type.basic_type == CT_STRUCT) {
					if (size_so_far % 8) {
						memory += size_so_far % 8;
						size_so_far += size_so_far % 8;
					}
				} else {
					const size_t size = ctype_size[member_type.basic_type];
					if (size_so_far % size) {
						memory += size_so_far % size;
						size_so_far += size_so_far % size;
					}
				}

				const size_t val_size = construct_cval_into(
					member_type,
					struct_member_values->val,
					memory
				);
				size_so_far += val_size;
				memory += val_size;

				struct_member_types = struct_member_types->next;
				struct_member_values = struct_member_values->next;
			}
			assert(struct_member_values == NULL);

			/*if (size_so_far >= 8) {
				fprintf(stderr, "Constructed struct of size %lu with first eight bytes: %lx\n", size_so_far, *(u64*)orig_mem);
			} else if (size_so_far == 4) {
				fprintf(stderr, "Constructed struct of size %lu with first four bytes: %x\n", size_so_far, *(uint32_t*)orig_mem);
			}*/

			return size_so_far;
		} break;
	}

	assert(false && "unreachable");
}

struct lisp_val destruct_cval_from(struct ctype type, void *memory, size_t *size) {
	switch (type.basic_type) {
		case CT_I8: {
			*size = ctype_size[CT_I8];
			return (struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(int8_t*)memory,
			};
		} break;
		case CT_U8: {
			*size = ctype_size[CT_U8];
			return (struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(uint8_t*)memory,
			};
		} break;
		case CT_I32: {
			*size = ctype_size[CT_I32];
			return (struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(int32_t*)memory,
			};
		} break;
		case CT_U32: {
			*size = ctype_size[CT_U32];
			return (struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(uint32_t*)memory,
			};
		} break;
		case CT_I64: {
			*size = ctype_size[CT_I64];
			return (struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(i64*)memory,
			};
		} break;
		case CT_U64: {
			*size = ctype_size[CT_U64];
			return (struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(i64*)memory,
			};
		} break;
		case CT_F32: {
			// TODO:
			fputs("Handling of C floats not yet implemented\n", stderr);
			exit(1);
		} break;
		case CT_F64: {
			// TODO:
			fputs("Handling of C floats not yet implemented\n", stderr);
			exit(1);
		} break;
		case CT_PTR: {
			// fprintf(stderr, "Constructing pointer from memory location %p to get value %p.\n", memory, *(void**)memory);
			*size = ctype_size[CT_PTR];
			list_t res = NULL;
			res = list_cons((struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(i64*)memory,
			}, res);
			res = list_cons((struct lisp_val) {
				.type = LT_SYM,
				.as.sym = sym_from_str("pointer"),
			}, res);
			return (struct lisp_val) {
				.type = LT_LIST,
				.as.list = res,
			};
		} break;
		case CT_VOID: {
			fputs("Void type cannot be constructed in memory!\n", stderr);
			exit(1);
		} break;
		case CT_STRUCT: {
			assert(false && "TODO");
		} break;
	}

	assert(false && "unreachable");
}

void *create_cval(struct ctype type, struct lisp_val from) {
	void *val;
	if (type.basic_type == CT_VOID) {
		fputs("Void type should never be passed as a parameter!\n", stderr);
		exit(1);
	} else if (type.basic_type == CT_STRUCT) {
		const size_t struct_size = get_struct_size(type.inner_desc.struct_members);

		val = malloc(struct_size);
	} else val = malloc(ctype_size[type.basic_type]);
	construct_cval_into(type, from, val);
	return val;
}

ffi_type *ctype_to_ffi_type(struct ctype ctype) {
	ffi_type *res = NULL;

	switch (ctype.basic_type) {
		case CT_I8: {
			res = &ffi_type_sint8;
		} break;
		case CT_U8: {
			res = &ffi_type_uint8;
		} break;
		case CT_I32: {
			res = &ffi_type_sint32;
		} break;
		case CT_U32: {
			res = &ffi_type_uint32;
		} break;
		case CT_I64: {
			res = &ffi_type_sint64;
		} break;
		case CT_U64: {
			res = &ffi_type_uint64;
		} break;
		case CT_F32: {
			res = &ffi_type_float;
		} break;
		case CT_F64: {
			res = &ffi_type_double;
		} break;
		case CT_PTR: {
			res = &ffi_type_pointer;
		} break;
		case CT_VOID: {
			res = &ffi_type_void;
		} break;
		case CT_STRUCT: {
			res = malloc(sizeof(*res));
			struct {
				ffi_type **items;
				size_t count;
				size_t capacity;
			} elements = {0};

			for (list_t member = ctype.inner_desc.struct_members; member != NULL; member = member->next) {
				struct ctype member_type = parse_ctype(member->val);
				ffi_type *ffi_type = ctype_to_ffi_type(member_type);
				da_append(&elements, ffi_type);
			}
			da_append(&elements, NULL);

			res->size = 0;
			res->alignment = 0;
			res->type = FFI_TYPE_STRUCT;
			res->elements = elements.items;
		} break;
	}

	return res;
}
void free_ffi_type(ffi_type *type) {
	if (type != NULL && type->type == FFI_TYPE_STRUCT) {
		for (ffi_type **st = type->elements; *st != NULL; ++st) {
			free_ffi_type(*st);
		}
		free(type->elements);
		free(type);
	}
}

struct lisp_val cval_to_lisp_val(struct ctype type, ffi_arg *cval) {
	switch (type.basic_type) {
		case CT_VOID: {
			return nil;
		} break;
		case CT_I8: {
			return (struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(int8_t*)cval,
			};
		} break;
		case CT_U8: {
			return (struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(uint8_t*)cval,
			};
		} break;
		case CT_I32: {
			return (struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(int32_t*)cval,
			};
		} break;
		case CT_U32: {
			return (struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(uint32_t*)cval,
			};
		} break;
		case CT_I64:
		case CT_U64:
		{
			return (struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(i64*)cval,
			};
		} break;
		case CT_F32: {
			fputs("error: ffi float handling not implemented yet!\n", stderr);
			exit(1);
		} break;
		case CT_F64: {
			fputs("error: ffi float handling not implemented yet!\n", stderr);
			exit(1);
		} break;
		case CT_PTR: {
			list_t res = NULL;
			res = list_cons((struct lisp_val) {
				.type = LT_NUM,
				.as.num = *(i64*)cval,
			}, res);
			res = list_cons((struct lisp_val) {
				.type = LT_SYM,
				.as.sym = sym_from_str("pointer"),
			}, res);
			return (struct lisp_val) {
				.type = LT_LIST,
				.as.list = res,
			};
		} break;
		case CT_STRUCT: {
			fputs("error: ffi struct handling not implemented yet!\n", stderr);
			exit(1);
		} break;
	}

	assert(false && "not implemented yet!");
}

struct call_res lffi_call(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->val.as.list != NULL);
	assert(args->val.as.list->val.type == LT_SYM);
	assert(strcmp(args->val.as.list->val.as.sym->sym, "csym") == 0);
	assert(args->val.as.list->next != NULL);
	assert(args->val.as.list->next->val.type == LT_NUM);
	assert(args->val.as.list->next->next == NULL);
	assert(args->next != NULL);

	void *func = *(void**)&args->val.as.list->next->val.as.num;

	ffi_cif cif;
	struct {
		ffi_type **items;
		size_t capacity;
		size_t count;
	} cargs = {0};
	struct {
		void **items;
		size_t capacity;
		size_t count;
	} cvals = {0};
	struct ctype return_type = parse_ctype(args->next->val);
	ffi_type *ffi_return_type = ctype_to_ffi_type(return_type);
	ffi_arg return_val;

	args = args->next->next;
	while (args) {
		assert(args->next != NULL);

		struct ctype arg_type = parse_ctype(args->val);
		void *arg = create_cval(arg_type, args->next->val);

		da_append(&cargs, ctype_to_ffi_type(arg_type));
		da_append(&cvals, arg);

		args = args->next->next;
	}

	struct lisp_val ret = nil;

	// fprintf(stderr, "Calling function %p with %lu arguments...\n", func, cargs.count);
	if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, cargs.count, ffi_return_type, cargs.items) != FFI_OK) {
		// function call failed
		// TODO: some sort of better error reporting mechanism?
		goto finish;
	}

	ffi_call(&cif, func, &return_val, cvals.items);

	ret = cval_to_lisp_val(return_type, &return_val);

finish:
	da_foreach(&cargs, ffi_type *, it) {
		free_ffi_type(*it);
	}
	da_free(&cargs);
	da_foreach(&cvals, void *, it) {
		free(*it);
	}
	da_free(&cvals);
	free_ffi_type(ffi_return_type);

	lret(ret);
}

struct call_res lstring_data_ptr(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING);
	assert(args->next == NULL);

	list_t res = NULL;
	res = list_cons((struct lisp_val) {
		.type = LT_NUM,
		.as.num = *(i64*)&args->val.as.string->data,
	 }, res);
	res = list_cons((struct lisp_val) {
		.type = LT_SYM,
		.as.sym = sym_from_str("pointer"),
	 }, res);

	lret(((struct lisp_val) {
		.type = LT_LIST,
		.as.list = res,
	}));
}

struct call_res lconstruct_val(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->val.as.list != NULL);
	assert(args->val.as.list->val.type == LT_SYM);
	assert(strcmp(args->val.as.list->val.as.sym->sym, "pointer") == 0);
	assert(args->val.as.list->next != NULL);
	assert(args->val.as.list->next->val.type == LT_NUM);
	assert(args->val.as.list->next->next == NULL);
	assert(args->next != NULL);
	assert(args->next->next != NULL);
	assert(args->next->next->next == NULL);

	struct ctype type = parse_ctype(args->next->val);

	const size_t size = construct_cval_into(
		type,
		args->next->next->val,
		*(void**)&args->val.as.list->next->val.as.num
	);

	lret(((struct lisp_val) {
		.type = LT_NUM,
		.as.num = *(i64*)&size,
	}));
}

struct call_res ldestruct_val(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->val.as.list != NULL);
	assert(args->val.as.list->val.type == LT_SYM);
	assert(strcmp(args->val.as.list->val.as.sym->sym, "pointer") == 0);
	assert(args->val.as.list->next != NULL);
	assert(args->val.as.list->next->val.type == LT_NUM);
	assert(args->val.as.list->next->next == NULL);
	assert(args->next != NULL);
	assert(args->next->next == NULL);

	struct ctype type = parse_ctype(args->next->val);

	size_t read_size = 0;
	struct lisp_val val = destruct_cval_from(
		type,
		*(void**)&args->val.as.list->next->val.as.num,
		&read_size
	);
	struct lisp_val val_size = (struct lisp_val) {
		.type = LT_NUM,
		.as.num = *(i64*)&read_size,
	};

	lret(((struct lisp_val) {
		.type = LT_LIST,
		.as.list = list_cons(val, list_cons(val_size, NULL)),
	}));
}

struct {
	const char *name;
	struct lbuiltin fn;
} BUILTINS[] = {
	{ "+", { ladd, true, NULL,
		"  num... -> the sum of the inputs"
	} },
	{ "-", { lsub, true, NULL,
		"  num -> the negation of the input\n"
		"  init num... -> all subsequent inputs subtracted from the first"
	} },
	{ "list", { llist_fn, true, NULL,
		"  (no arguments) -> nil\n"
		"  args... -> the list containing args as given\n"
		"  example: (list 1 2 (+ 3 4) (- 5)) -> (1 2 7 -5)"
	} },
	{ "cons", { lcons, true, NULL,
		"  head tail -> prepends `head` to the list `tail`\n"
		"  example: (cons 42 nil) -> (42) ; (cons ' a '(b)) -> (a b)"
	} },
	{ "append", { lappend, true, NULL,
		"  list val -> appends `val` to `list`"
	} },
	{ "quote", { lquote, false, NULL,
		"  val -> returns the value as-is\n"
		"  used to represent values literally rather than evaluating them"
	} },
	{ "eval", { leval, true, NULL,
		"  val -> evaluates the argument\n"
		"  note: (eval (quote <val>)) == <val>"
	} },
	{ "def", { ldef, false, NULL,
		"  name expr -> evaluates `expr` and defines the symbol `name` to point to the result"
	} },
	{ "assoc", { lassoc, false, NULL,
		"  (name1 val1 name2 val2...) expr -> creates a new parameter-environment with each name associated with the result of evaluating the paired value, and evaluates `expr` in that environment"
	} },
	{ "env-new", { lenv_new, false, NULL,
		"  expr -> creates a new, empty environment and evaluates `expr` in that environment"
	} },
	{ ":=", { lset, false, NULL,
		"  name val -> finds the binding of `name` in this environment or a parent environment, and re-binds it to the result of evaluating `val`, also giving that as the result"
	} },
	{ "truthy?", { ltruthy, true, NULL,
		"  val -> whether the val is considered truthy by `if`"
	} },
	{ "if", { lif, false, NULL,
		"  cond then else -> if `cond` evaluates to T, runs `then`, otherwise runs `else`"
	} },
	{ "do", { ldo, false, NULL,
		"  val... -> evaluates each argument in turn, returning the result"
	} },
	// do implemented in lisp:
	// (defm mydo (() vals) (if (and ' vals (tail ' vals)) (and (or (eval (head ' vals)) T) (eval (cons ' mydo (tail ' vals)))) (if ' vals (eval (head ' vals)))))
	// (defm mydo (() vals) (if (and ' vals (tail ' vals)) (and (or (eval (head ' vals)) T) (call mydo (tail ' vals))) (if ' vals (eval (head ' vals)))))
	{ "call", { lcall, true, NULL,
		"  callable args -> call the callable with the given arguments; prevents re-evaluation of arguments by a function, and can be used to pass evaluated arguments to a macro"
	} },
	{ "pstr", { lpstr, true, NULL,
		"  str -> prints the string to stdout, or ASCII character if the argument is a number"
	} },
	// could also be implemented as:
	// (defn pstr (str) (write stdout str))
	{ "head", { lhead, true, NULL,
		"  list -> the first element of the list"
	} },
	{ "tail", { ltail, true, NULL,
		"  list -> the list without the first element"
	} },
	{ "nth", { lnth, true, NULL,
		"  list n -> the nth element of the list"
	} },
	// could also be implemented as:
	// (defn nth (list n) (if n (nth (tail list) (- n 1)) (head list)))
	{ "subs", { lsubs, true, NULL,
		"  value -> substitutes any occurence of a symbol in `value` which is defined in the current environmebt, with its associated value"
	} },
	{ "subs-with", { lsubs_with, false, NULL,
		"  (name1 val1 name2 val2...) body -> associates each name with the associated value as in (assoc), then substitutes the evaluated `body` as if with (subs), but using only the newly-defined bindings rather than the outer environment"
	} },
	{ "/", { l_div, true, NULL,
		"  a b... -> the first argument divided by each of the subsequent arguments in turn"
	} },
	{ "%", { lmod, true, NULL,
		"  a b -> a modulo b"
	} },
	{ "cmp", { lcmp, true, NULL,
		"  a b -> -1 if a<b, 0 if a==b, or 1 if a>b"
	} },
	{ "<<", { llsh, true, NULL,
		"  a b -> a left-shifted by b bits"
	} },
	{ ">>", { lrsh, true, NULL,
		"  a b -> a right-shifted by b bits"
	} },
	{ "~", { lbnot, true, NULL,
		"  num -> bitwise negation"
	} },
	{ "&", { lband, true, NULL,
		"  a b -> bitwise and"
	} },
	{ "|", { lbor, true, NULL,
		"  a b -> bitwise or"
	} },
	{ "^", { lbxor, true, NULL,
		"  a b -> bitwise exclusive or"
	} },
	// (defm not (x) (if x F T))
	{ "and", { land, false, NULL,
		"  a b... -> evaluates each of its arguments in turn, returning the first falsey argument (leaving the rest unevaluated) or otherwise the last argument"
	} },
	{ "or", { lor, false, NULL,
		"  a b... -> evaluates each of its arguments in turn, returning the first truthy argument (leaving the rest unevaluated) or otherwise the last argument"
	} },
	{ "type", { ltype, true, NULL,
		"  val -> the type of the value, as a symbol (number, builtin, symbol, list, boolean, string)"
	} },
	{ "exit", { lexit, true, NULL,
		"  (no arguments) -> exits the program with exit code 0\n"
		"  exitcode -> exits the program with the specified exit code"
	} },
	{ "&$", { ljoin_s, true, NULL,
		"  args... -> joins all its arguments into one string, which should be strings, or numbers interpreted as characters"
	} },
	{ "[]$", { lsubstr_s, true, NULL,
		"  str idx -> the character at index `idx` (a number)\n"
		"  str start stop -> the substring with first character at index `start` and last character just preceding index `stop`"
	} },
	{ ":refs", { lrefs, true, NULL,
		"  val -> the number of references a garbage-collected value (strings, symbols, or lists) has, or nil"
	} },
	{ ":id", { lid, true, NULL,
		"  val -> the unique id (memory location) of a garbage-collected value (strings, symbols, or lists), or nil"
	} },
	{ "repr", { lrepr, true, NULL,
		"  val -> the string representation of the given value, such that (nth (parse (repr <val>)) 1) == <val>"
	} },
	{ "parse", { lparse, true, NULL,
		"  str -> attempts to parse a lisp value from a string, giving (rest-of-string value) on success, or just (rest-of-string) on failure"
	} },
	{ "open", { lopen, true, NULL,
		"  filename mode -> opens the given file with the given mode, giving a file object"
	} },
	{ "close", { lclose, true, NULL,
		"  file -> closes the specified file"
	} },
	{ "readline", { lreadline, true, NULL,
		"  file -> a line of text from the file, without the trailing newline, or nil on eof"
	} },
	{ "read", { lread, true, NULL,
		"  file -> the contents of the file as a string"
	} },
	{ "write", { lwrite, true, NULL,
		"  file data -> write the data to the file, the data being a string or a number interpreted as a character"
	} },
	{ ":name", { lname, true, NULL,
		"  callable -> a callable's name, or nil if it doesn't have one"
	} },
	{ ":docs", { ldocs, true, NULL,
		"  callable -> a callable's docstring, or nil if it doesn't have one"
	} },
	{ ":macro?", { lis_macro, true, NULL,
		"  callable -> T if the callable is a macro, F if it is a function"
	} },
	{ ":callable?", { lis_callable, true, NULL,
		"  val -> T if the value is a callable (builtin or correctly-structured list), F otherwise"
	} },
	{ "len$", { llen_s, true, NULL,
		"  string -> the length of the string"
	} },
	{ "!ffi-load", { lffi_load, true, NULL,
		"  libname -> tries to load the specified C library, returning (' clib <id>) on success or a string containing the error on failure"
	} },
	{ "!ffi-unload", { lffi_unload, true, NULL,
		"  library -> unloads the given C library"
	} },
	{ "!ffi-sym", { lffi_sym, true, NULL,
		"  library fnname -> tries to load the given name from the given C library, returning (' csym <id>) on success or a string containing the error on failure"
	} },
	{ "!ffi-call", { lffi_call, true, NULL,
		"  csym ret-type args... -> calls the given C function with the given return type and the given arguments, arguments given first as a type, then a value\n"
		"  The following C types are supported:\n"
		"    i8    8-bit signed integer, value is a number\n"
		"    u8    8-bit unsigned integer, value is a number\n"
		"    i32   32-bit signed integer, value is a number\n"
		"    u32   32-bit unsigned integer, value is a number\n"
		"    i64   64-bit signed integer, value is a number\n"
		"    u64   64-bit unsigned integer, value is a number\n"
		"    f32   32-bit floating-point number, value is a number\n"
		"    f64   64-bit floating-point number, value is a number\n"
		"    ptr   pointer, value is a pair (' ptr number)\n"
		"    void  no value; should only be used as a return type, gives () as a result\n"
		"    (...) a structure with elements of the given types, value is a list with the same format"
	} },
	{ "!string-data-pointer", { lstring_data_ptr, true, NULL,
		"  string -> (pointer <pointer-to-string-data)"
	} },
	{ "!construct-val", { lconstruct_val, true, NULL,
		"  pointer type value -> construct the C type from the given value in the specified memory location"
	} },
	{ "!destruct-val", { ldestruct_val, true, NULL,
		"  pointer type -> dereference the pointer of the given type, returning the value"
	} },
};

struct lisp_val last_res;
void destroy_envs(int n) {
	while (n) {
		assert(curr_env->parent != NULL);
		struct env *env = curr_env;
		curr_env = env->parent;
		env_free(env);
		--n;
	}
}
struct lisp_val eval(struct lisp_val val) {
	int envs = 0;
	bool tailcall = false;

	val = lisp_val_copy(val);

recurse:
	switch (val.type) {
		case LT_SYM: {
			if (strcmp(val.as.sym->sym, "_") == 0) {
				destroy_envs(envs);
				lisp_val_free(val);
				return lisp_val_copy(last_res);
			}

			struct assoc *assoc = find_var(curr_env, val.as.sym);
			if (assoc != NULL) {
				struct lisp_val res = lisp_val_copy(assoc->value);
				destroy_envs(envs);
				lisp_val_free(val);
				return res;
			}

			for (size_t i = 0; i < sizeof(BUILTINS)/sizeof(*BUILTINS); ++i) {
				if (strcmp(val.as.sym->sym, BUILTINS[i].name) == 0) {
					destroy_envs(envs);
					lisp_val_free(val);
					return (struct lisp_val) {
						.type = LT_BUILTIN,
						.as.builtin = BUILTINS[i].fn,
					};
				}
			}

			fprintf(stderr, "undefined symbol `%s`\n", val.as.sym->sym);
			destroy_envs(envs);
			lisp_val_free(val);
			return (struct lisp_val) {
				.type = LT_LIST,
				.as.list = NULL,
			};
		} break;
		case LT_LIST: {
			if (val.as.list == NULL) {
				destroy_envs(envs);
				return val;
			}

			struct lisp_val fn = eval(val.as.list->val);
			if (fn.type != LT_BUILTIN && fn.type != LT_LIST) {
				fputs("error: tried calling value ", stderr);
				lisp_val_print(fn, stderr);
				fputs(" as function\n", stderr);
				destroy_envs(envs);
				lisp_val_free(val);
				lisp_val_free(fn);
				return lisp_val_copy(nil);
			}

			struct call_res res;
			if (fn.type == LT_BUILTIN) {
				 res = lb_call(fn.as.builtin, val.as.list->next, false);
			} else {
				res = ll_call(fn.as.list, val.as.list->next, tailcall, false);
			}
			lisp_val_free(val);
			lisp_val_free(fn);
			if (!res.eval) {
				assert(!res.destroy_env);
				destroy_envs(envs);
				return res.val;
			}

			if (res.destroy_env) ++envs;
			val = res.val;
			tailcall = true;
			goto recurse;
		} break;
		default:
			destroy_envs(envs);
			return val;
	}

	assert(false && "unreachable");
}
struct call_res lb_call(struct lbuiltin fn, list_t args, bool pre_evald) {
	if (fn.eval_args) {
		list_t processed_args = NULL;
		while (args != NULL) {
			struct lisp_val tmp = pre_evald
				? lisp_val_copy(args->val)
				: eval(args->val);
			processed_args = list_append(processed_args, tmp);
			lisp_val_free(tmp);
			args = args->next;
		}

		struct call_res res = fn.fn(processed_args);
		list_free(processed_args);
		return res;
	} else {
		return fn.fn(args);
	}
}
struct lisp_val substitute(struct lisp_val into, struct env *from) {
	switch (into.type) {
		case LT_SYM: {
			struct assoc *found = find_var(from, into.as.sym);
			if (found == NULL) return lisp_val_copy(into);
			else return lisp_val_copy(found->value);
		} break;
		case LT_LIST: {
			list_t lst = into.as.list;
			list_t res = NULL;

			while (lst != NULL) {
				struct lisp_val tmp = substitute(lst->val, from);
				res = list_append(res, tmp);
				lisp_val_free(tmp);

				lst = lst->next;
			}

			return (struct lisp_val) {
				.type = LT_LIST,
				.as.list = res,
			};
		} break;
		default: return lisp_val_copy(into);
	}
}
struct call_res ll_call(list_t fn, list_t args, bool tailcall, bool pre_evald) {
	assert(list_is_fn(fn));

	struct list_fn lfn = list_to_fn(fn);
	list_t params = lfn.params;

	if (lfn.is_macro) {
		struct env env = {0};

		while (params != NULL) {
			if (params->val.type == LT_LIST) {
				env_def(&env, params->next->val.as.sym, (struct lisp_val) {
					.type = LT_LIST,
					.as.list = args,
				});

				args = NULL;
				break;
			}

			assert(params->val.type == LT_SYM);

			if (args == NULL) {
				fputs("not enough arguments provided!\n", stderr);
				break;
			}

			env_def(&env, params->val.as.sym, args->val);

			params = params->next;
			args = args->next;
		}

		if (args != NULL) {
			fputs("too many arguments provided!\n", stderr);
		}

		struct lisp_val body = substitute(lfn.body, &env);

		env_clear(&env);
		list_fn_free(lfn);

		return (struct call_res) {
			.val = body,
			.eval = true,
			.destroy_env = false,
		};
	} else {
		bool replace_env = tailcall && curr_env->params_of == fn;

		struct env *env = malloc(sizeof(struct env));
		*env = (struct env) {0};
		env->parent = curr_env;
		env->fixed = true;
		env->params_of = list_copy(fn);

		while (params != NULL) {
			if (params->val.type == LT_LIST) {
				list_t val = NULL;

				while (args) {
					struct lisp_val tmp = pre_evald
						? lisp_val_copy(args->val)
						: eval(args->val);
					val = list_append(val, tmp);
					lisp_val_free(tmp);

					args = args->next;
				}

				env_def(env, params->next->val.as.sym, (struct lisp_val) {
					.type = LT_LIST,
					.as.list = val,
				});

				list_free(val);

				args = NULL;
				break;
			}

			assert(params->val.type == LT_SYM);

			if (args == NULL) {
				fputs("not enough arguments provided!\n", stderr);
				break;
			}

			struct lisp_val tmp = pre_evald
				? lisp_val_copy(args->val)
				: eval(args->val);
			env_def(env, params->val.as.sym, tmp);
			lisp_val_free(tmp);

			params = params->next;
			args = args->next;
		}

		if (args != NULL) {
			fputs("too many arguments provided!\n", stderr);
		}

		if (replace_env) {
			assert(env->count == curr_env->count);
			env->parent = curr_env->parent;
			env_free(curr_env);
			curr_env = env;
		} else {
			curr_env = env;
		}

		struct lisp_val body = lisp_val_copy(lfn.body);
		list_fn_free(lfn);

		return (struct call_res) {
			.val = body,
			.eval = true,
			.destroy_env = !replace_env,
		};
	}
}

void run_file(const char *fname) {
	errno = 0;

	FILE *f = fopen(fname, "r");
	if (errno || !f) {
		fprintf(stderr, "error while processing file %s: ", fname);
		perror("while opening file");
		exit(1);
	}

	size_t size = 1024;
	size_t len = 0;
	char *data = malloc(sizeof(char)*size);

	for (;;) {
		size_t amt = fread(&data[len], 1, size - len, f);
		if (amt < size - len) {
			len += amt;
			if (ferror(f)) {
				fprintf(stderr, "error while processing file %s: ", fname);
				perror("while reading file");
				exit(1);
			} else break;
		} else {
			len = size;
			size *= 2;
			data = realloc(data, sizeof(char)*size);
		}
	}

	fclose(f);
	const char *begin = data;
	for (;;) {
		skip_ws(&begin, &len);
		if (!len) goto finish;
		struct lisp_val parsed = lisp_val_parse(&begin, &len);
		struct lisp_val evald = eval(parsed);
		lisp_val_free(parsed);
		lisp_val_free(last_res);
		last_res = evald;
	}

finish:
	free(data);
}
void run_repl(void) {
	enum input_status input_status;
	for (;;) {
		fputs("> ", stdout);
		input_status = input(&line_builder, stdin);
		if (input_status == IS_EOF && line_builder.count == 0) {
			break;
		}

		line = line_builder.items;
		line_len = line_builder.count;

		// cast &line to (void*) to side-step incorrect constness warnings from the compiler
		struct lisp_val parsed = lisp_val_parse((void*)&line, &line_len);
		struct lisp_val evald = eval(parsed);

		lisp_val_free(parsed);
		lisp_val_free(last_res);

		last_res = evald;
		lisp_val_print(last_res, stdout);
		putchar('\n');
	}
}

void helptext(const char *prog, FILE *f) {
	fprintf(f, "rholisp interpreter.\n");
	fputc('\n', f);
	fprintf(f, "Usage:\n");
	fprintf(f, "%s [options]\n", prog);
	fprintf(f, "    run interactive repl\n");
	fprintf(f, "%s [options] -- [args]\n", prog);
	fprintf(f, "    run interactive repl with command line arguments\n");
	fprintf(f, "%s [options] <file> [args]\n", prog);
	fprintf(f, "    run rholisp script file\n");
	fputc('\n', f);
	fprintf(f, "Options:\n");
	fprintf(f, "    --help, -help, -h    print this help text and exit\n");
	fprintf(f, "    -nostd               do not load the standard library\n");
	fprintf(f, "                         before the script/repl\n");
	fprintf(f, "    -preload <file>      run the specified file before\n");
	fprintf(f, "                         the script/repl\n");
}

int main(int argc, char **argv) {
	for (size_t i = 0; i < sizeof(BUILTINS)/sizeof(*BUILTINS); ++i) {
		BUILTINS[i].fn.name = sym_from_str(BUILTINS[i].name);
	}

	last_res = lisp_val_copy(nil);

	env_add(&root_env, (struct assoc) {
		.name = sym_from_str("nil"),
		.value = nil,
	});

	env_add(&root_env, (struct assoc) {
		.name = sym_from_str("stdin"),
		.value = file_to_lisp_val(stdin),
	});
	env_add(&root_env, (struct assoc) {
		.name = sym_from_str("stdout"),
		.value = file_to_lisp_val(stdout),
	});
	env_add(&root_env, (struct assoc) {
		.name = sym_from_str("stderr"),
		.value = file_to_lisp_val(stderr),
	});

	bool nostd = false;
	bool *preloads = malloc(sizeof(bool)*argc);
	memset(preloads, 0, sizeof(bool)*argc);
	char *program_name = NULL;
	int prog_args_start = argc;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-nostd") == 0) {
			nostd = true;
		} else if (strcmp(argv[i], "-preload") == 0) {
			++i;
			assert(i < argc);
			preloads[i] = true;
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-help") == 0) {
			helptext(argv[0], stdout);
			return 1;
		} else if (strcmp(argv[i], "--") == 0) {
			prog_args_start = i+1;
			break;
		} else if (strncmp(argv[i], "-", 1) == 0) {
			helptext(argv[0], stderr);
			fprintf(stderr, "unknown command-line switch %s\n", argv[i]);
			exit(1);
		} else {
			program_name = argv[i];
			prog_args_start = i+1;
			break;
		}
	}

	list_t args = NULL;
	if (program_name) {
		string_t tmp = string_from_str(program_name);
		args = list_append(args, (struct lisp_val) {
			.type = LT_STRING,
			.as.string = tmp,
		});
		string_free(tmp);
	} else {
		args = list_append(args, nil);
	}
	for (int i = prog_args_start; i < argc; ++i) {
		string_t tmp = string_from_str(argv[i]);
		args = list_append(args, (struct lisp_val) {
			.type = LT_STRING,
			.as.string = tmp,
		});
		string_free(tmp);
	}
	env_add(&root_env, (struct assoc) {
		.name = sym_from_str("args"),
		.value = (struct lisp_val) {
			.type = LT_LIST,
			.as.list = args,
		},
	});

	if (!nostd) {
		run_file("std.lisp");
	}
	for (int i = 0; i < argc; ++i) {
		if (preloads[i]) run_file(argv[i]);
	}
	free(preloads);

	if (program_name) {
		run_file(program_name);
	} else {
		run_repl();
	}

	lisp_val_free(last_res);
}
