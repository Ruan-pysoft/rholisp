#include <ctype.h>
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
			(da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items)); \
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
			(da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items)); \
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
	const builtin_t this,
	const lisp_list_t args,
	struct _builtin_call_opts opts
);
#define builtin_call(this, args, ...) _builtin_call( \
	this, \
	args, \
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
	const list_t this,
	const lisp_list_t args,
	struct _list_call_opts opts
);
#define list_call(this, args, ...) _list_call( \
	this, \
	args, \
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
	(void) lhs;
	(void) rhs;
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
	(void) this;
	return true;
}

// + misc +

// NOTE: _builtin_call defined below

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
	(void) this;
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
	(void) lhs;
	(void) rhs;
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
	if (this == NULL) return;
	assert(this->refcount != 0);
	++this->refcount;
	assert(this->info->net_refcount != 0);
	++this->info->net_refcount;
}
void list_decrefs(lisp_list_t this) {
	if (this == NULL) return;
	assert(this->refcount != 0);
	--this->refcount;
	assert(this->info->net_refcount != 0);
	--this->info->net_refcount;
	if (this->refcount == 0) {
		// I think this is what I have to do??
		if (this->next != NULL) ++this->info->net_refcount;
		list_destroy(this);
	}
}
void list_destroy(lisp_list_t this) {
	assert(this->info != NULL);

	value_decrefs(this->val);
	if (this->next) list_decrefs(this->next);
	else {
		assert(this == this->info->last);
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
	assert(this->info->net_refcount == 1);

	list_t new_last = malloc(sizeof(*new_last));
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
	assert(this->info->net_refcount == 1);

	assert(false && "TODO");
}

// + misc +

// NOTE: _list_call defined below

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
	(void) lhs;
	(void) rhs;
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

/* SECTION: CONSTANTS */

#define LIST_OF_PREDEFINED_SYMBOLS \
	X(quote,   "quote") \
	X(nil,     "nil") \
	X(number,  "number") \
	X(builtin, "builtin") \
	X(symbol,  "symbol") \
	X(list,    "list") \
	X(boolean, "boolean") \
	X(string,  "string") \
	X(file,    "file") \
	X(clib,    "clib") \
	X(csym,    "csym") \
	X(pointer, "pointer") \
	X(value,   "value") \
	X(error,   "error") \
	X(eof,     "eof")

#define X(name, _) static lisp_value_t name ## _symbol;
LIST_OF_PREDEFINED_SYMBOLS
#undef X

/* SECTION: PARSING (DECLARATION) */

struct file_position {
	size_t line;
	const char *line_start;
	const char *pos;
};

// WARNING: Assumes pos is still within the file!
void file_position_advance(struct file_position *this);
// WARNING: Assumes pos will still be within the file!
struct file_position file_position_after(struct file_position fp, size_t adv_by);

enum parsed_item_type {
	PIT_OK,
	PIT_ERR,
	PIT_UNMATCHED_PAREN,
	PIT_UNMATCHED_QUOTE,
	PIT_INVALID_ESCAPE,
	PIT_OVERFLOW,
	PIT_NOCHAR,

	PIT_EOF
};

// like a mix between a token and an ast
struct parsed_item {
	struct file_position pos;
	struct file_position end;

	enum parsed_item_type type;
	union {
		lisp_value_t value;
		char *error;
	} as;
};

struct parsed_item parsed_item_error(
	struct file_position pos, struct file_position end,
	enum parsed_item_type err_t, char *msg
);
struct parsed_item parsed_item_value(
	struct file_position pos, struct file_position end, lisp_value_t value
);
void parsed_item_destroy(struct parsed_item *this);

struct parsed_item quoted_item(struct file_position pos, struct parsed_item item);

struct parser {
	const char *file_name;
	const char *file;
	size_t file_size;
	struct file_position pos;
};

struct parser parser_from_strn(const char *name, const char *str, size_t n);
struct parser parser_from_str(const char *name, const char *str);
struct parser parser_from_sb(const char *name, struct string_builder sb);

bool parser_isatend(const struct parser *this);
void parser_advance(struct parser *this);
int parser_peek(struct parser *this);
int parser_peeknext(struct parser *this);

bool is_symchar(int ch);
bool isnot_symchar(int ch);
void parser_skip_ws_or_comment(struct parser *this);

struct parsed_item parser_next_list(struct parser *this);
struct parsed_item parser_next_string(struct parser *this);
struct parsed_item parser_next_number(struct parser *this);
struct parsed_item parser_next_char(struct parser *this);
struct parsed_item parser_next_symbol(struct parser *this);
struct parsed_item parser_next(struct parser *this);

/* SECTION: PARSING (DEFINITION) */

void file_position_advance(struct file_position *this) {
	if (*(this->pos++) == '\n') {
		this->line_start = this->pos;
		++this->line;
	}
}
struct file_position file_position_after(struct file_position fp, size_t adv_by) {
	struct file_position res = fp;
	for (size_t i = 0; i < adv_by; ++i) {
		file_position_advance(&res);
	}
	return res;
}

struct parsed_item parsed_item_error(
	struct file_position pos, struct file_position end,
	enum parsed_item_type err_t, char *msg
) {
	return (struct parsed_item) {
		.pos = pos,
		.end = end,

		.type = err_t,
		.as.error = msg,
	};
}
struct parsed_item parsed_item_value(
	struct file_position pos, struct file_position end, lisp_value_t value
) {
	return (struct parsed_item) {
		.pos = pos,
		.end = end,

		.type = PIT_OK,
		.as.value = value,
	};
}
void parsed_item_destroy(struct parsed_item *this) {
	if (this->type == PIT_OK) {
		value_decrefs(this->as.value);
	} else {
		free(this->as.error);
	}
}

struct parsed_item quoted_item(struct file_position pos, struct parsed_item item) {
	if (item.type != PIT_OK) {
		return item;
	}

	list_t lst = NULL;
	lst = list_cons(item.as.value, lst);
	lst = list_cons(quote_symbol, lst);
	value_decrefs(item.as.value);

	return parsed_item_value(pos, item.end, value_of_list(lst));
}

struct parser parser_from_strn(const char *name, const char *str, size_t n) {
	return (struct parser) {
		.file_name = name,
		.file = str,
		.file_size = n,
		.pos = {
			.line = 1,
			.line_start = str,
			.pos = str,
		},
	};
}
struct parser parser_from_str(const char *name, const char *str) {
	return parser_from_strn(name, str, strlen(str));
}
struct parser parser_from_sb(const char *name, struct string_builder sb) {
	return parser_from_strn(name, sb.items, sb.count);
}

bool parser_isatend(const struct parser *this) {
	return this->pos.pos >= this->file + this->file_size;
}
void parser_advance(struct parser *this) {
	if (!parser_isatend(this)) {
		file_position_advance(&this->pos);
	}
}
int parser_peek(struct parser *this) {
	return parser_isatend(this) ? -1 : *this->pos.pos;
}
int parser_peeknext(struct parser *this) {
	return this->pos.pos + 1 < this->file + this->file_size
		? this->pos.pos[1]
		: -1;
}

bool is_symchar(int ch) {
	return !isnot_symchar(ch) && ' ' < ch && ch < '\x7f';
}
bool isnot_symchar(int ch) {
	return isspace(ch) || ch == '(' || ch == ')' || ch == ';' || ch == '"';
}
void parser_skip_ws_or_comment(struct parser *this) {
	while (isspace(parser_peek(this)) || parser_peek(this) == ';') {
		if (parser_peek(this) == ';') {
			while (!parser_isatend(this)
			&& parser_peek(this) != '\n') {
				parser_advance(this);
			}
		}
		parser_advance(this);
	}
}

struct parsed_item parser_next_list(struct parser *this) {
	parser_skip_ws_or_comment(this);

	const struct file_position start_pos = this->pos;

	assert(parser_peek(this) == '(');
	parser_advance(this);

	list_t res = NULL;

	for (;;) {
		parser_skip_ws_or_comment(this);
		if (parser_isatend(this)) {
			return parsed_item_error(
				start_pos, this->pos, PIT_UNMATCHED_PAREN,
				strdup("expected closing parenthesis before EOF")
			);
		}
		if (parser_peek(this) == ')') {
			parser_advance(this);
			return parsed_item_value(
				start_pos, this->pos,
				value_of_list(res)
			);
		}

		struct parsed_item tmp = parser_next(this);
		if (tmp.type != PIT_OK) {
			list_decrefs(res);
			return tmp;
		}
		res = list_append(res, tmp.as.value);
		parsed_item_destroy(&tmp);
	}
}
struct parsed_item parser_next_string(struct parser *this) {
	parser_skip_ws_or_comment(this);

	const struct file_position start_pos = this->pos;

	assert(parser_peek(this) == '"');
	parser_advance(this);

	const char *str_begin = this->pos.pos;

	while (!parser_isatend(this) && parser_peek(this) != '"') {
		if (parser_peek(this) == '\\') parser_advance(this);
		if (parser_peek(this) == '\n') {
			return parsed_item_error(
				start_pos, this->pos, PIT_UNMATCHED_QUOTE,
				strdup("expected closing quote before newline")
			);
		}
		parser_advance(this);
	}

	if (parser_isatend(this)) {
		return parsed_item_error(
			start_pos, this->pos, PIT_UNMATCHED_QUOTE,
			strdup("expected closing quote before EOF")
		);
	}

	const char *str_end = this->pos.pos;

	assert(parser_peek(this) == '"');
	parser_advance(this);

	string_t res = malloc(sizeof(*res));
	*res = (struct string) {
		.data = malloc(sizeof(char)*(str_end-str_begin)),
		.len = 0,

		.borrows = NULL,
		.refcount = 1,
	};

	for (const char *c = str_begin; c < str_end; ++c) {
		if (*c == '\\') {
			const struct file_position before_bslash = file_position_after(
				start_pos, 1 + (c - str_begin)
			);

			++c;
			for (size_t i = 0; i < sizeof(escapes)/sizeof(*escapes); ++i) {
				if (escapes[i][1] == *c) {
					res->data[res->len++] = escapes[i][0];
					goto escaped;
				}
			}

			const struct file_position after_escapecode = file_position_after(
				before_bslash, 2
			);

			free(res->data);
			free(res);
			return parsed_item_error(
				before_bslash, after_escapecode, PIT_INVALID_ESCAPE,
				strdup("unrecognised escape code")
			);
		escaped:;
		} else {
			res->data[res->len++] = *c;
		}
	}

	return parsed_item_value(start_pos, this->pos, value_of_string(res));
}
struct parsed_item parser_next_number(struct parser *this) {
	parser_skip_ws_or_comment(this);

	const struct file_position start_pos = this->pos;

	const bool negative = parser_peek(this) == '-';
	if (negative) parser_advance(this);
	u64 res = 0;

	while (isdigit(parser_peek(this))) {
		const u64 oldres = res;
		res *= 10;
		res += parser_peek(this) - '0';

		if (res < oldres) {
			return parsed_item_error(
				start_pos, this->pos, PIT_OVERFLOW,
				strdup("integer magnitude too large to represent in 64-bit number")
			);
		}

		parser_advance(this);
	}

	if (!negative && res == (1ul << 63)) {
		return parsed_item_error(
			start_pos, this->pos, PIT_OVERFLOW,
			strdup("2**63 not representable as a positive signed 64-bit number")
		);
	}

	if (negative) res = 1+~res;

	return parsed_item_value(
		start_pos, this->pos,
		value_of_number(*(i64*)res)
	);
}
struct parsed_item parser_next_char(struct parser *this) {
	parser_skip_ws_or_comment(this);

	const struct file_position start_pos = this->pos;

	assert(parser_peek(this) == '#');
	parser_advance(this);
	assert(!is_symchar(parser_peek(this)));
	
	while (isspace(parser_peek(this))) parser_advance(this);

	if (parser_isatend(this)) {
		return parsed_item_error(
			start_pos, this->pos, PIT_NOCHAR,
			strdup("expected character after #, got EOF")
		);
	}

	const char ch = parser_peek(this);
	const struct file_position before_bslash = this->pos;
	parser_advance(this);

	if (ch == '\\') {
		if (parser_isatend(this)) {
			return parsed_item_error(
				start_pos, this->pos, PIT_NOCHAR,
				strdup("expected escape code, got EOF")
			);

			const char e = parser_peek(this);
			parser_advance(this);

			for (size_t i = 0; i < sizeof(escapes)/sizeof(*escapes); ++i) {
				if (escapes[i][1] == e) {
					return parsed_item_value(
						start_pos, this->pos,
						value_of_number(escapes[i][0])
					);
				}
			}

			return parsed_item_error(
				before_bslash, this->pos, PIT_INVALID_ESCAPE,
				strdup("unrecognised escape code")
			);
		}
	}

	return parsed_item_value(
		start_pos, this->pos,
		value_of_number(ch)
	);
}
struct parsed_item parser_next_symbol(struct parser *this) {
	parser_skip_ws_or_comment(this);

	const struct file_position start_pos = this->pos;

	if (parser_isatend(this)) {
		return parsed_item_error(
			this->pos, this->pos, PIT_EOF,
			strdup("expected a value, but got EOF")
		);
	}

	while (is_symchar(parser_peek(this))) {
		parser_advance(this);
	}

	return parsed_item_value(
		start_pos, this->pos,
		value_of_symbol(symbol_from_strn(
			start_pos.pos, this->pos.pos-start_pos.pos
		))
	);
}
struct parsed_item parser_next(struct parser *this) {
	parser_skip_ws_or_comment(this);
	if (parser_isatend(this)) {
		return parsed_item_error(
			this->pos, this->pos, PIT_EOF,
			strdup("expected a value, but got EOF")
		);
	}

	if (parser_peek(this) == '(') {
		return parser_next_list(this);
	} else if (parser_peek(this) == '"') {
		return parser_next_string(this);
	} else if (isdigit(parser_peek(this))) {
		return parser_next_number(this);
	} else if (parser_peek(this) == '-' && isdigit(parser_peeknext(this))) {
		return parser_next_number(this);
	} else if ((parser_peek(this) == 'T' || parser_peek(this) == 'F')
	&& !is_symchar(parser_peeknext(this))) {
		const struct file_position pos = this->pos;
		const lisp_value_t value = value_of_boolean(parser_peek(this) == 'T');
		parser_advance(this);
		return parsed_item_value(pos, this->pos, value);
	} else if (parser_peek(this) == '\'' && !is_symchar(parser_peeknext(this))) {
		const struct file_position pos = this->pos;
		parser_advance(this);
		return quoted_item(pos, parser_next(this));
	} else if (parser_peek(this) == '#' && !is_symchar(parser_peeknext(this))) {
		return parser_next_char(this);
	} else {
		return parser_next_symbol(this);
	}
}

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

struct env root_env = {0};
struct env *curr_env = &root_env;

/* SECTION: TODO REWRITE */

struct list_fn {
	list_t params;
	bool is_macro;
	symbol_t name;
	string_t doc;
	lisp_value_t body;
};
struct list_fn list_fn_copy(struct list_fn this) {
	list_copy(this.params);
	if (this.name != NULL) symbol_increfs(this.name);
	if (this.doc != NULL) string_increfs(this.doc);
	value_increfs(this.body);

	return this;
}
void list_fn_free(struct list_fn this) {
	list_decrefs(this.params);
	if (this.name != NULL) symbol_decrefs(this.name);
	if (this.doc != NULL) string_decrefs(this.doc);
	value_decrefs(this.body);
}
bool list_is_fn(list_t list) {
	if (list == NULL) return false;
	if (list->val.type != LT_LIST) return false;

	list_t params = list->val.as.list;
	while (params != NULL) {
		if (params->val.type == LT_SYMBOL) {
			params = params->next;
			continue;
		}

		if (params->val.type != LT_LIST) return false;
		if (params->next == NULL) return false;
		if (params->next->val.type != LT_SYMBOL) return false;
		if (params->next->next != NULL) return false;
		break;
	}

	if (list->next == NULL) return false;
	if (list->next->val.type == LT_LIST) {
		if (list->next->next == NULL) return false;
		if (list->next->next->val.type != LT_BOOLEAN) return false;
		if (list->next->next->next == NULL) return false;
		if (list->next->next->next->next != NULL) return false;

		list_t meta = list->next->val.as.list;

		if (meta == NULL) return false;
		if (meta->val.type != LT_SYMBOL) return false;
		if (meta->next == NULL) return false;
		if (meta->next->val.type != LT_STRING) return false;
		if (meta->next->next != NULL) return false;

		return true;
	} else if (list->next->val.type == LT_BOOLEAN) {
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
		.name = symbol_copy(list->next->val.as.list->val.as.symbol),
		.doc = string_copy(list->next->val.as.list->next->val.as.string),
		.is_macro = list->next->next->val.as.boolean,
		.body = value_copy(list->next->next->next->val),
	}; else res = (struct list_fn) {
		.params = list_copy(list->val.as.list),
		.name = NULL,
		.doc = NULL,
		.is_macro = list->next->val.as.boolean,
		.body = value_copy(list->next->next->val),
	};

	return res;
}

/* SECTION: FUNCTION CALLING */

lisp_value_t eval(lisp_value_t val);
struct call_res _builtin_call(
	const builtin_t this,
	const lisp_list_t args,
	struct _builtin_call_opts opts
) {
	if (this->eval_args && !opts.inhibit_argument_evaluation) {
		list_t processed_args = NULL;
		list_t curr = args;
		while (curr != NULL) {
			lisp_value_t tmp = eval(curr->val);
			processed_args = list_append(processed_args, tmp);
			value_decrefs(tmp);
			curr = curr->next;
		}

		struct call_res res = this->fn(processed_args);
		list_decrefs(processed_args);
		return res;
	} else {
		return this->fn(args);
	}
}
lisp_value_t substitute(lisp_value_t into, struct env *from);
struct call_res _list_call(
	const list_t this,
	const lisp_list_t args,
	struct _list_call_opts opts
) {
	assert(list_is_fn(this));

	struct list_fn lfn = list_to_fn(this);
	list_t params = lfn.params;

	if (lfn.is_macro) {
		struct env env = {0};

		list_t curr_arg = args;
		while (params != NULL) {
			if (params->val.type == LT_LIST) {
				env_def(&env, params->next->val.as.symbol, value_of_list(curr_arg));

				curr_arg = NULL;
				break;
			}

			assert(params->val.type == LT_SYMBOL);

			if (args == NULL) {
				fputs("not enough arguments provided!\n", stderr);
				break;
			}

			env_def(&env, params->val.as.symbol, curr_arg->val);

			params = params->next;
			curr_arg = curr_arg->next;
		}

		if (curr_arg != NULL) {
			fputs("too many arguments provided!\n", stderr);
		}

		lisp_value_t body = substitute(lfn.body, &env);

		env_clear(&env);
		list_fn_free(lfn);

		return call_res_from(body, .eval = true);
	} else {
		const bool replace_env = opts.is_tail_call && curr_env->params_of == this;

		struct env *env = malloc(sizeof(*env));
		*env = (struct env) {0};
		env->parent = curr_env;
		env->fixed = true;
		env->params_of = list_copy(this);

		list_t curr_arg = args;
		while (params != NULL) {
			if (params->val.type == LT_LIST) {
				list_t val = NULL;

				while (curr_arg) {
					lisp_value_t tmp = opts.inhibit_argument_evaluation
						? value_copy(curr_arg->val)
						: eval(curr_arg->val);
					val = list_append(val, tmp);
					value_decrefs(tmp);

					curr_arg = curr_arg->next;
				}
				env_def(env, params->next->val.as.symbol, value_of_list(val));

				list_decrefs(val);

				curr_arg = NULL;
				break;
			}

			assert(params->val.type == LT_SYMBOL);

			if (args == NULL) {
				fputs("not enough arguments provided!\n", stderr);
				break;
			}

			lisp_value_t tmp = opts.inhibit_argument_evaluation
				? value_copy(curr_arg->val)
				: eval(curr_arg->val);
			env_def(env, params->val.as.symbol, tmp);
			value_decrefs(tmp);

			params = params->next;
			curr_arg = curr_arg->next;
		}

		if (curr_arg != NULL) {
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

		lisp_value_t body = value_copy(lfn.body);
		list_fn_free(lfn);

		return call_res_from(body, .eval=true, .destroy_env=!replace_env);
	}
}

/* SECTION: REFACTOR PENDING */

struct call_res ladd(list_t args) {
	assert(args != NULL);

	i64 res = 0;

	while (args != NULL) {
		assert(args->val.type == LT_NUMBER);
		res += args->val.as.number;

		args = args->next;
	}

	return call_res_from(value_of_number(res));
}
struct call_res lsub(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUMBER);

	if (args->next == NULL) {
		return call_res_from(value_of_number(-args->val.as.number));
	}

	i64 res = args->val.as.number;
	args = args->next;

	while (args != NULL) {
		assert(args->val.type == LT_NUMBER);
		res -= args->val.as.number;

		args = args->next;
	}

	return call_res_from(value_of_number(res));
}
struct call_res llist_fn(list_t args) {
	return call_res_from(value_of_list(list_copy(args)));
}
struct call_res lcons(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);
	assert(args->next->next == NULL);
	assert(args->next->val.type == LT_LIST);

	return call_res_from(value_of_list(list_cons(args->val, args->next->val.as.list)));
}
struct call_res lappend(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next->next == NULL);

	//fprintf(stderr, "Appending to list with %lu refs\n", args->val.as.list->info->net_refcount);
	return call_res_from(value_of_list(list_append(
		list_dup(args->val.as.list),
		args->next->val
	)));
}
struct call_res lappend_to(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_SYMBOL);
	assert(args->next != NULL);
	assert(args->next->next == NULL);

	struct assoc *assoc = find_var(curr_env, args->val.as.symbol);
	assert(assoc != NULL);
	assert(assoc->value.type == LT_LIST);

	//fprintf(stderr, "In-place append to list with %lu refs\n", assoc->value.as.list->info->net_refcount);

	lisp_value_t tmp = eval(args->next->val);
	if (assoc->value.as.list->info->net_refcount == 1) {
		//fprintf(stderr, "Modifying list in-place...\n");
		list_append(assoc->value.as.list, tmp);
	} else {
		//fprintf(stderr, "Duplicating list...\n");
		lisp_value_t old_value = assoc->value;
		assoc->value = value_of_list(list_append(
			list_dup(old_value.as.list),
			tmp
		));
		value_decrefs(old_value);
	}
	value_decrefs(tmp);

	return call_res_from(value_copy(assoc->value));
}
struct call_res lquote(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	return call_res_from(value_copy(args->val));
}
lisp_value_t eval(lisp_value_t val);
struct call_res leval(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	return call_res_from(value_copy(args->val), .eval = true);
}
struct call_res ldef(list_t args) {
	while (args != NULL) {
		assert(args->val.type == LT_SYMBOL);
		assert(args->next != NULL);

		struct env *env = curr_env;
		while (env->fixed) {
			assert(env->parent != NULL);
			env = env->parent;
		}
		lisp_value_t tmp = eval(args->next->val);
		env_def(env, args->val.as.symbol, tmp);
		value_decrefs(tmp);

		args = args->next->next;
	}

	return call_res_from(nil);
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
		assert(vars->val.type == LT_SYMBOL);
		assert(vars->next != NULL);

		lisp_value_t tmp = eval(vars->next->val);
		env_def(curr_env, vars->val.as.symbol, tmp);
		value_decrefs(tmp);

		vars = vars->next->next;
	}

	return call_res_from(value_copy(args->next->val), .eval = true, .destroy_env = true);
}
struct call_res lenv_new(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	struct env *env = malloc(sizeof(struct env));
	*env = (struct env) {0};
	env->parent = curr_env;
	curr_env = env;

	return call_res_from(value_copy(args->val), .eval = true, .destroy_env = true);
}
struct call_res lset(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_SYMBOL);
	assert(args->next != NULL);
	assert(args->next->next == NULL);

	struct assoc *assoc = find_var(curr_env, args->val.as.symbol);
	assert(assoc != NULL);

	// freeing assoc->val immediately caused me *so* much pain when I tried to set a string to its own substring...
	lisp_value_t tmp = assoc->value;
	assoc->value = eval(args->next->val);
	value_decrefs(tmp);

	return call_res_from(value_copy(assoc->value));
}
struct call_res ltruthy(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	return call_res_from(value_of_boolean(value_is_truthy(args->val)));
}
struct call_res lif(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);
	assert(args->next->next == NULL || args->next->next->next == NULL);

	lisp_value_t tmp = eval(args->val);
	bool cond = value_is_truthy(tmp);
	value_decrefs(tmp);

	if (cond) {
		return call_res_from(value_copy(args->next->val), .eval = true);
	} else if (args->next->next != NULL) {
		return call_res_from(value_copy(args->next->next->val), .eval = true);
	} else {
		return call_res_from(nil);
	}
}
struct call_res ldo(list_t args) {
	if (args == NULL) return call_res_from(nil);

	while (args->next != NULL) {
		value_decrefs(eval(args->val));
		args = args->next;
	}

	return call_res_from(value_copy(args->val), .eval = true);
}

struct call_res lcall(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_BUILTIN || args->val.type == LT_LIST);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_LIST);
	assert(args->next->next == NULL);

	if (args->val.type == LT_BUILTIN) {
		return builtin_call(
			args->val.as.builtin,
			args->next->val.as.list,
			.inhibit_argument_evaluation = true,
		);
	} else {
		return list_call(
			args->val.as.list,
			args->next->val.as.list,
			.inhibit_argument_evaluation = true,
			.is_tail_call = false,
		);
	}
}
struct call_res lpstr(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING || args->val.type == LT_NUMBER);
	assert(args->next == NULL);

	if (args->val.type == LT_STRING) {
		printf("%.*s", (int)args->val.as.string->len, args->val.as.string->data);
	} else {
		assert(0 <= args->val.as.number && args->val.as.number < 256);

		putchar(args->val.as.number);
	}

	return call_res_from(nil);
}
struct call_res lhead(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next == NULL);

	assert(args->val.as.list != NULL);
	return call_res_from(value_copy(args->val.as.list->val));
}
struct call_res ltail(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next == NULL);

	assert(args->val.as.list != NULL);
	return call_res_from(value_of_list(list_copy(args->val.as.list->next)));
}
struct call_res lnth(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUMBER);
	assert(args->next->next == NULL);

	list_t lst = args->val.as.list;
	for (i64 i = 0; i < args->next->val.as.number; ++i) {
		assert(lst != NULL);
		lst = lst->next;
	}

	assert(lst != NULL);
	return call_res_from(value_copy(lst->val));
}
lisp_value_t substitute(lisp_value_t into, struct env *from);
struct call_res lsubs(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	return call_res_from(substitute(args->val, curr_env));
}
struct call_res lsubs_with(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next != NULL);
	assert(args->next->next == NULL);

	struct env env = {0};

	list_t subs = args->val.as.list;

	while (subs != NULL) {
		assert(subs->val.type == LT_SYMBOL);
		assert(subs->next != NULL);

		lisp_value_t tmp = eval(subs->next->val);
		env_def(&env, subs->val.as.symbol, tmp);
		value_decrefs(tmp);

		subs = subs->next->next;
	}

	lisp_value_t tmp = eval(args->next->val);
	lisp_value_t res = substitute(tmp, &env);
	value_decrefs(tmp);

	env_clear(&env);

	return call_res_from(res);
}

struct call_res l_div(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUMBER);
	assert(args->next != NULL);

	i64 res = args->val.as.number;
	args = args->next;

	while (args != NULL) {
		assert(args->val.type == LT_NUMBER);
		res /= args->val.as.number;

		args = args->next;
	}

	return call_res_from(value_of_number(res));
}

struct call_res lmod(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUMBER);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUMBER);
	assert(args->next->next == NULL);

	return call_res_from(value_of_number(args->val.as.number % args->next->val.as.number));
}

i64 sign(i64 n) {
	return n < 0 ? -1 : n == 0 ? 0 : 1;
}
struct call_res lcmp(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);
	assert(args->next->next == NULL);

	return call_res_from(value_of_number(value_cmp(args->val, args->next->val)));
}

struct call_res llsh(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUMBER);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUMBER);
	assert(args->next->next == NULL);

	return call_res_from(value_of_number(args->val.as.number << args->next->val.as.number));
}

struct call_res lrsh(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUMBER);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUMBER);
	assert(args->next->next == NULL);

	return call_res_from(value_of_number(args->val.as.number >> args->next->val.as.number));
}

struct call_res lbnot(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUMBER);
	assert(args->next == NULL);

	return call_res_from(value_of_number(~args->val.as.number));
}

struct call_res lband(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUMBER);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUMBER);
	assert(args->next->next == NULL);

	return call_res_from(value_of_number(args->val.as.number & args->next->val.as.number));
}

struct call_res lbor(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUMBER);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUMBER);
	assert(args->next->next == NULL);

	return call_res_from(value_of_number(args->val.as.number | args->next->val.as.number));
}

struct call_res lbxor(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_NUMBER);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUMBER);
	assert(args->next->next == NULL);

	return call_res_from(value_of_number(args->val.as.number ^ args->next->val.as.number));
}

struct call_res land(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);

	while (args->next != NULL) {
		lisp_value_t res = eval(args->val);
		if (!value_is_truthy(res)) return call_res_from(res);
		value_decrefs(res);
		args = args->next;
	}

	return call_res_from(value_copy(args->val), .eval = true);
}

struct call_res lor(list_t args) {
	assert(args != NULL);
	assert(args->next != NULL);

	while (args->next != NULL) {
		lisp_value_t res = eval(args->val);
		if (value_is_truthy(res)) return call_res_from(res);
		value_decrefs(res);
		args = args->next;
	}

	return call_res_from(value_copy(args->val), .eval = true);
}

struct call_res ltype(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	switch (args->val.type) {
#define X(type_name, enum_name) \
		case enum_name: return call_res_from(value_copy(type_name ## _symbol));
		LIST_OF_LISP_TYPES
#undef X
	}

	assert(false && "unreachable");
}

struct call_res lexit(list_t args) {
	assert(args == NULL || args->next == NULL);

	if (args != NULL) {
		assert(args->val.type == LT_NUMBER);

		// you can get valgrind to shut up about that 56 bytes of leaked memory when you call exit by uncommenting the following line:
		//list_free(args);
		// however, that breaks the gc model I have (it's not lexit's responsibility to free its args), also I don't know why valgrind thinks it's lost, since it's still somewhere in the callstack
		exit(args->val.as.number);
	} else exit(0);
}
struct call_res ljoin_s(list_t args) {
	assert(args != NULL);

	size_t strlen = 0;
	for (list_t args_it = args; args_it != NULL; args_it = args_it->next) {
		assert(args_it->val.type == LT_STRING || args_it->val.type == LT_NUMBER);
		assert(args_it->val.type != LT_NUMBER || (0 <= args_it->val.as.number && args_it->val.as.number < 256));

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
			res->data[res->len++] = args->val.as.number;
		}

		args = args->next;
	}
	assert(res->len == strlen);

	return call_res_from(value_of_string(res));
}
struct call_res lsubstr_s(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_NUMBER);
	assert(args->next->next == NULL || args->next->next->val.type == LT_NUMBER);
	assert(args->next->next == NULL || args->next->next->next == NULL);

	if (args->next->next == NULL) {
		assert(0 <= args->next->val.as.number && args->next->val.as.number < (i64)args->val.as.string->len);

		return call_res_from(value_of_number(args->val.as.string->data[args->next->val.as.number]));
	} else {
		string_t s = args->val.as.string;
		const i64 begin = args->next->val.as.number;
		const i64 end = args->next->next->val.as.number;

		assert(begin <= end);
		assert(0 <= begin);
		assert(end <= (i64)s->len);

		return call_res_from(value_of_string(string_substr(s, begin, end)));
	}
}
struct call_res lrefs(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	switch (args->val.type) {
#define X(type_name, enum_type) \
		case enum_type: { \
			return call_res_from(value_of_number(args->val.as.type_name->refcount)); \
		} break;
		LIST_OF_GCD_TYPES
#undef X
		default: return call_res_from(nil);
	}

	assert(false && "unreachable");
}
struct call_res lid(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	if (args->val.type == LT_SYMBOL)  {
		return call_res_from(value_of_number(*(i64*)&args->val.as.symbol));
	} else if (args->val.type == LT_LIST) {
		return call_res_from(value_of_number(*(i64*)&args->val.as.list));
	} else if (args->val.type == LT_STRING) {
		return call_res_from(value_of_number(*(i64*)&args->val.as.string));
	} else if (args->val.type == LT_BUILTIN) {
		return call_res_from(value_of_number(*(i64*)&args->val.as.builtin->fn));
	}

	return call_res_from(nil);
}

struct call_res lrepr(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	struct string_builder sb = {0};
	value_repr(args->val, &sb);

	return call_res_from(value_of_string(sb_to_string(&sb)));
}

list_t create_parse_error(struct parsed_item item) {
	assert(item.type != PIT_OK);

	list_t error = NULL;

	string_t error_msg = string_from_str(item.as.error);
	error = list_cons(value_of_string(error_msg), error);
	string_decrefs(error_msg);

	string_t place = string_from_strn(item.pos.pos, item.end.pos - item.pos.pos);
	error = list_cons(value_of_string(place), error);
	string_decrefs(place);

	switch (item.type) {
		case PIT_OK: assert(false && "unreachable");
		case PIT_ERR: {
			string_t sort = string_from_str("error");
			error = list_cons(value_of_string(sort), error);
			string_decrefs(sort);
		} break;
		case PIT_UNMATCHED_PAREN: {
			string_t sort = string_from_str("unmatched opening parenthesis");
			error = list_cons(value_of_string(sort), error);
			string_decrefs(sort);
		} break;
		case PIT_UNMATCHED_QUOTE: {
			string_t sort = string_from_str("unmatched quote");
			error = list_cons(value_of_string(sort), error);
			string_decrefs(sort);
		} break;
		case PIT_INVALID_ESCAPE: {
			string_t sort = string_from_str("invalid escape code");
			error = list_cons(value_of_string(sort), error);
			string_decrefs(sort);
		} break;
		case PIT_OVERFLOW: {
			string_t sort = string_from_str("overflow while parsing number");
			error = list_cons(value_of_string(sort), error);
			string_decrefs(sort);
		} break;
		case PIT_NOCHAR: {
			string_t sort = string_from_str("no character found following #");
			error = list_cons(value_of_string(sort), error);
			string_decrefs(sort);
		} break;
		case PIT_EOF: {
			string_t sort = string_from_str("end of file");
			error = list_cons(value_of_string(sort), error);
			string_decrefs(sort);
		} break;
	}

	error = list_cons(value_of_number(item.pos.line), error);
	if (item.type == PIT_EOF) error = list_cons(eof_symbol, error);
	else error = list_cons(error_symbol, error);

	return error;
}
struct call_res lparse(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING);
	assert(args->next == NULL);

	list_t res = NULL;

	const char *str = args->val.as.string->data;
	size_t len = args->val.as.string->len;

	struct parser parser = parser_from_strn("<string>", str, len);
	struct parsed_item item = parser_next(&parser);

	string_t tmp = string_substr(
		args->val.as.string,
		parser.pos.pos - args->val.as.string->data,
		args->val.as.string->len
	);
	res = list_cons(value_of_string(tmp), res);
	string_decrefs(tmp);

	if (item.type == PIT_OK) {
		list_t value = NULL;
		value = list_cons(item.as.value, value);
		value = list_cons(value_symbol, value);
		res = list_cons(value_of_list(value), res);
		list_decrefs(value);
	} else {
		list_t error = create_parse_error(item);

		res = list_cons(value_of_list(error), res);
		list_decrefs(error);
	}

	parsed_item_destroy(&item);

	return call_res_from(value_of_list(res));
}

lisp_value_t file_to_lisp_val(FILE *f) {
	list_t res = NULL;
	res = list_append(res, file_symbol);
	res = list_append(res, value_of_number(*(i64*)&f));

	return value_of_list(res);
}
FILE *lisp_val_to_file(lisp_value_t v) {
	assert(v.type == LT_LIST);
	assert(v.as.list != NULL);
	assert(v.as.list->val.type == LT_SYMBOL);
	assert(strcmp(v.as.list->val.as.symbol->sym, "file") == 0);
	assert(v.as.list->next != NULL);
	assert(v.as.list->next->val.type == LT_NUMBER);
	assert(v.as.list->next->next == NULL);

	return *(FILE**)&v.as.list->next->val.as.number;
}

struct call_res lopen(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING || args->val.type == LT_SYMBOL);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_STRING || args->next->val.type == LT_SYMBOL);
	assert(args->next->next == NULL);

	const char *fname = args->val.type == LT_STRING
		? strndup(args->val.as.string->data, args->val.as.string->len)
		: args->val.as.symbol->sym
	;
	const char *fmode = args->next->val.type == LT_STRING
		? strndup(args->next->val.as.string->data, args->next->val.as.string->len)
		: args->next->val.as.symbol->sym
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

	return call_res_from(file_to_lisp_val(res));
}

struct call_res lclose(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next == NULL);

	FILE *f = lisp_val_to_file(args->val);
	fclose(f);

	return call_res_from(nil);
}

struct call_res lreadline(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next == NULL);

	FILE *f = lisp_val_to_file(args->val);

	struct string_builder sb = {0};

	enum input_status is = input(&sb, f);

	if (is == IS_EOF && sb.count == 0) {
		return call_res_from(nil);
	}

	return call_res_from(value_of_string(sb_to_string(&sb)));
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

	return call_res_from(value_of_string(sb_to_string(&sb)));
}

struct call_res lwrite(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_STRING || args->next->val.type == LT_NUMBER);
	assert(args->next->next == NULL);

	FILE *f = lisp_val_to_file(args->val);

	if (args->next->val.type == LT_NUMBER) {
		assert(0 <= args->next->val.as.number && args->next->val.as.number < 256);

		fputc(args->next->val.as.number, f);
	} else {
		const size_t written = fwrite(args->next->val.as.string->data, 1, args->next->val.as.string->len, f);
		assert(written == args->next->val.as.string->len);
	}

	return call_res_from(nil);
}

struct call_res lname(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST || args->val.type == LT_BUILTIN);
	assert(args->next == NULL);

	if (args->val.type == LT_LIST) {
		list_t lfn = args->val.as.list;
		assert(list_is_fn(lfn));

		struct list_fn fn = list_to_fn(lfn);

		if (fn.name == NULL) return call_res_from(nil);
		else {
			symbol_t name = symbol_copy(fn.name);
			list_fn_free(fn);
			return call_res_from(value_of_symbol(name));
		}
	} else {
		builtin_t fn = args->val.as.builtin;

		if (fn->name != NULL) {
			return call_res_from(value_of_symbol(symbol_copy(fn->name)));
		} else return call_res_from(nil);
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

		if (fn.name == NULL) return call_res_from(nil);
		else {
			string_t doc = string_copy(fn.doc);
			list_fn_free(fn);
			return call_res_from(value_of_string(doc));
		}
	} else {
		builtin_t fn = args->val.as.builtin;

		if (fn->doc != NULL) {
			return call_res_from(value_of_string(string_from_str(fn->doc)));
		} else return call_res_from(nil);
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

		return call_res_from(value_of_boolean(fn.is_macro));
	} else {
		builtin_t fn = args->val.as.builtin;

		return call_res_from(value_of_boolean(!fn->eval_args));
	}
}

struct call_res lis_callable(list_t args) {
	assert(args != NULL);
	assert(args->next == NULL);

	return call_res_from(value_of_boolean(
		args->val.type == LT_BUILTIN || (args->val.type == LT_LIST && list_is_fn(args->val.as.list))
	));
}

struct call_res llen_s(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING);
	assert(args->next == NULL);

	return call_res_from(value_of_number(args->val.as.string->len));
}

struct call_res lffi_load(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING);
	assert(args->next == NULL);

	char *libname = strndup(args->val.as.string->data, args->val.as.string->len);
	void *lib = dlopen(libname, RTLD_LAZY | RTLD_GLOBAL);
	free(libname);

	if (lib == NULL) {
		return call_res_from(value_of_string(string_from_str(dlerror())));
	} else {
		list_t res = NULL;
		res = list_cons(value_of_number(*(i64*)&lib), res);
		res = list_cons(value_copy(clib_symbol), res);
		return call_res_from(value_of_list(res));
	}
}

struct call_res lffi_unload(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->val.as.list != NULL);
	assert(args->val.as.list->val.type == LT_SYMBOL);
	assert(strcmp(args->val.as.list->val.as.symbol->sym, "clib") == 0);
	assert(args->val.as.list->next != NULL);
	assert(args->val.as.list->next->val.type == LT_NUMBER);
	assert(args->val.as.list->next->next == NULL);
	assert(args->next == NULL);

	dlclose(*(void**)&args->val.as.list->next->val.as.number);

	return call_res_from(nil);
}

struct call_res lffi_sym(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->val.as.list != NULL);
	assert(args->val.as.list->val.type == LT_SYMBOL);
	assert(strcmp(args->val.as.list->val.as.symbol->sym, "clib") == 0);
	assert(args->val.as.list->next != NULL);
	assert(args->val.as.list->next->val.type == LT_NUMBER);
	assert(args->val.as.list->next->next == NULL);
	assert(args->next != NULL);
	assert(args->next->val.type == LT_STRING);
	assert(args->next->next == NULL);

	dlerror();
	char *symname = strndup(args->next->val.as.string->data, args->next->val.as.string->len);
	void *sym = dlsym(*(void**)&args->val.as.list->next->val.as.number, symname);
	free(symname);
	const char *err = NULL;
	if (sym == NULL && (err = dlerror()) != NULL) {
		return call_res_from(value_of_string(string_from_str(err)));
	} else {
		list_t res = NULL;
		res = list_cons(value_of_number(*(i64*)&sym), res);
		res = list_cons(value_copy(csym_symbol), res);
		return call_res_from(value_of_list(res));
	}

	return call_res_from(nil);
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

struct ctype parse_ctype(lisp_value_t val) {
	if (val.type != LT_SYMBOL && val.type != LT_LIST) {
		fputs("C types should either be a symbol, nil, or a list of C types!\n", stderr);
		exit(1);
	}

	if (val.type == LT_SYMBOL) {
		if (strcmp(val.as.symbol->sym, "i8") == 0) {
			return (struct ctype) { .basic_type = CT_I8 };
		} else if (strcmp(val.as.symbol->sym, "u8") == 0) {
			return (struct ctype) { .basic_type = CT_U8 };
		} else if (strcmp(val.as.symbol->sym, "i32") == 0) {
			return (struct ctype) { .basic_type = CT_I32 };
		} else if (strcmp(val.as.symbol->sym, "u32") == 0) {
			return (struct ctype) { .basic_type = CT_U32 };
		} else if (strcmp(val.as.symbol->sym, "i64") == 0) {
			return (struct ctype) { .basic_type = CT_I64 };
		} else if (strcmp(val.as.symbol->sym, "u64") == 0) {
			return (struct ctype) { .basic_type = CT_U64 };
		} else if (strcmp(val.as.symbol->sym, "f32") == 0) {
			return (struct ctype) { .basic_type = CT_F32 };
		} else if (strcmp(val.as.symbol->sym, "f64") == 0) {
			return (struct ctype) { .basic_type = CT_F64 };
		} else if (strcmp(val.as.symbol->sym, "ptr") == 0) {
			return (struct ctype) { .basic_type = CT_PTR };
		} else {
			fprintf(stderr, "Unknown C type `%s`\n", val.as.symbol->sym);
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

size_t construct_cval_into(struct ctype type, lisp_value_t from, void *memory) {
	switch (type.basic_type) {
		case CT_I8: {
			assert(from.type == LT_NUMBER);
			assert(-(1<<7) <= from.as.number && from.as.number < (1<<7));

			*(int8_t*)memory = from.as.number;
			return ctype_size[CT_I8];
		} break;
		case CT_U8: {
			assert(from.type == LT_NUMBER);
			assert(0 <= from.as.number && from.as.number < (1<<8));

			*(uint8_t*)memory = from.as.number;
			return ctype_size[CT_U8];
		} break;
		case CT_I32: {
			assert(from.type == LT_NUMBER);
			assert(-(1l<<31) <= from.as.number && from.as.number < (1l<<31));

			*(int32_t*)memory = from.as.number;
			return ctype_size[CT_I32];
		} break;
		case CT_U32: {
			assert(from.type == LT_NUMBER);
			assert(0 <= from.as.number && from.as.number < (1l<<32));

			*(uint32_t*)memory = from.as.number;
			return ctype_size[CT_U32];
		} break;
		case CT_I64: {
			assert(from.type == LT_NUMBER);

			*(i64*)memory = from.as.number;
			return ctype_size[CT_I64];
		} break;
		case CT_U64: {
			assert(from.type == LT_NUMBER);

			*(u64*)memory = *(u64*)&from.as.number;
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
			assert(from.as.list->val.type == LT_SYMBOL);
			assert(strcmp(from.as.list->val.as.symbol->sym, "pointer") == 0);
			assert(from.as.list->next->val.type == LT_NUMBER);
			assert(from.as.list->next->next == NULL);

			*(void**)memory = *(void**)&from.as.list->next->val.as.number;
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

lisp_value_t destruct_cval_from(struct ctype type, void *memory, size_t *size) {
	switch (type.basic_type) {
		case CT_I8: {
			*size = ctype_size[CT_I8];
			return value_of_number(*(int8_t*)memory);
		} break;
		case CT_U8: {
			*size = ctype_size[CT_U8];
			return value_of_number(*(uint8_t*)memory);
		} break;
		case CT_I32: {
			*size = ctype_size[CT_I32];
			return value_of_number(*(int32_t*)memory);
		} break;
		case CT_U32: {
			*size = ctype_size[CT_U32];
			return value_of_number(*(uint32_t*)memory);
		} break;
		case CT_I64: {
			*size = ctype_size[CT_I64];
			return value_of_number(*(i64*)memory);
		} break;
		case CT_U64: {
			*size = ctype_size[CT_U64];
			return value_of_number(*(i64*)memory);
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
			res = list_cons(value_of_number(*(i64*)memory), res);
			res = list_cons(pointer_symbol, res);
			return value_of_list(res);
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

void *create_cval(struct ctype type, lisp_value_t from) {
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

lisp_value_t cval_to_lisp_val(struct ctype type, ffi_arg *cval) {
	switch (type.basic_type) {
		case CT_VOID: {
			return nil;
		} break;
		case CT_I8: {
			return value_of_number(*(int8_t*)cval);
		} break;
		case CT_U8: {
			return value_of_number(*(uint8_t*)cval);
		} break;
		case CT_I32: {
			return value_of_number(*(int32_t*)cval);
		} break;
		case CT_U32: {
			return value_of_number(*(uint32_t*)cval);
		} break;
		case CT_I64:
		case CT_U64:
		{
			return value_of_number(*(i64*)cval);
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
			res = list_cons(value_of_number(*(i64*)cval), res);
			res = list_cons(pointer_symbol, res);
			return value_of_list(res);
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
	assert(args->val.as.list->val.type == LT_SYMBOL);
	assert(strcmp(args->val.as.list->val.as.symbol->sym, "csym") == 0);
	assert(args->val.as.list->next != NULL);
	assert(args->val.as.list->next->val.type == LT_NUMBER);
	assert(args->val.as.list->next->next == NULL);
	assert(args->next != NULL);

	void *func = *(void**)&args->val.as.list->next->val.as.number;

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

	lisp_value_t ret = nil;

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

	return call_res_from(ret);
}

struct call_res lstring_data_ptr(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_STRING);
	assert(args->next == NULL);

	list_t res = NULL;
	res = list_cons(value_of_number(*(i64*)&args->val.as.string->data), res);
	res = list_cons(pointer_symbol, res);

	return call_res_from(value_of_list(res));
}

struct call_res lconstruct_val(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->val.as.list != NULL);
	assert(args->val.as.list->val.type == LT_SYMBOL);
	assert(strcmp(args->val.as.list->val.as.symbol->sym, "pointer") == 0);
	assert(args->val.as.list->next != NULL);
	assert(args->val.as.list->next->val.type == LT_NUMBER);
	assert(args->val.as.list->next->next == NULL);
	assert(args->next != NULL);
	assert(args->next->next != NULL);
	assert(args->next->next->next == NULL);

	struct ctype type = parse_ctype(args->next->val);

	const size_t size = construct_cval_into(
		type,
		args->next->next->val,
		*(void**)&args->val.as.list->next->val.as.number
	);

	return call_res_from(value_of_number(*(i64*)&size));
}

struct call_res ldestruct_val(list_t args) {
	assert(args != NULL);
	assert(args->val.type == LT_LIST);
	assert(args->val.as.list != NULL);
	assert(args->val.as.list->val.type == LT_SYMBOL);
	assert(strcmp(args->val.as.list->val.as.symbol->sym, "pointer") == 0);
	assert(args->val.as.list->next != NULL);
	assert(args->val.as.list->next->val.type == LT_NUMBER);
	assert(args->val.as.list->next->next == NULL);
	assert(args->next != NULL);
	assert(args->next->next == NULL);

	struct ctype type = parse_ctype(args->next->val);

	size_t read_size = 0;
	lisp_value_t val = destruct_cval_from(
		type,
		*(void**)&args->val.as.list->next->val.as.number,
		&read_size
	);
	lisp_value_t val_size = value_of_number(*(i64*)&read_size);

	return call_res_from(value_of_list(
		list_cons(val, list_cons(val_size, NULL))
	));
}

struct {
	const char *name;
	struct builtin fn;
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
	{ "append-to", { lappend_to, false, NULL,
		"  name val -> appends `val` to the list associated with the symbol `name`\n"
		"  NOTE: if the association is the only reference to the list value, append-to mutates the underlying value, rather than constructing a new list, yielding performance improvements"
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

lisp_value_t last_res;
void destroy_envs(int n) {
	while (n) {
		assert(curr_env->parent != NULL);
		struct env *env = curr_env;
		curr_env = env->parent;
		env_free(env);
		--n;
	}
}
lisp_value_t eval(lisp_value_t val) {
	int envs = 0;
	bool tailcall = false;

	value_increfs(val);

recurse:
	switch (val.type) {
		case LT_SYMBOL: {
			if (strcmp(val.as.symbol->sym, "_") == 0) {
				destroy_envs(envs);
				value_decrefs(val);
				return value_copy(last_res);
			}

			struct assoc *assoc = find_var(curr_env, val.as.symbol);
			if (assoc != NULL) {
				lisp_value_t res = value_copy(assoc->value);
				destroy_envs(envs);
				value_decrefs(val);
				return res;
			}

			for (size_t i = 0; i < sizeof(BUILTINS)/sizeof(*BUILTINS); ++i) {
				if (strcmp(val.as.symbol->sym, BUILTINS[i].name) == 0) {
					destroy_envs(envs);
					value_decrefs(val);
					return value_of_builtin(&BUILTINS[i].fn);
				}
			}

			fprintf(stderr, "undefined symbol `%s`\n", val.as.symbol->sym);
			destroy_envs(envs);
			value_decrefs(val);
			return nil;
		} break;
		case LT_LIST: {
			if (val.as.list == NULL) {
				destroy_envs(envs);
				return val;
			}

			lisp_value_t fn = eval(val.as.list->val);
			if (fn.type != LT_BUILTIN && fn.type != LT_LIST) {
				struct string_builder sb = {0};
				value_repr(fn, &sb);
				fprintf(stderr, "error: tried calling value %.*s as function\n", (int)sb.count, sb.items);
				sb_clear(&sb);
				destroy_envs(envs);
				value_decrefs(val);
				value_decrefs(fn);
				return nil;
			}

			struct call_res res;
			if (fn.type == LT_BUILTIN) {
				res = builtin_call(fn.as.builtin, val.as.list->next);
			} else {
				res = list_call(fn.as.list, val.as.list->next, .is_tail_call=tailcall);
			}
			value_decrefs(val);
			value_decrefs(fn);
			if (!res.eval) {
				assert(!res.destroy_env);
				destroy_envs(envs);
				return res.result;
			}

			if (res.destroy_env) ++envs;
			val = res.result;
			tailcall = true;
			goto recurse;
		} break;
		default:
			destroy_envs(envs);
			return val;
	}

	assert(false && "unreachable");
}
lisp_value_t substitute(lisp_value_t into, struct env *from) {
	switch (into.type) {
		case LT_SYMBOL: {
			struct assoc *found = find_var(from, into.as.symbol);
			if (found == NULL) return value_copy(into);
			else return value_copy(found->value);
		} break;
		case LT_LIST: {
			list_t lst = into.as.list;
			list_t res = NULL;

			while (lst != NULL) {
				lisp_value_t tmp = substitute(lst->val, from);
				res = list_append(res, tmp);
				value_decrefs(tmp);

				lst = lst->next;
			}

			return value_of_list(res);
		} break;
		default: return value_copy(into);
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
	struct parser parser = parser_from_strn(fname, data, len);
	for (;;) {
		parser_skip_ws_or_comment(&parser);
		if (parser_isatend(&parser)) goto finish;
		struct parsed_item item = parser_next(&parser);
		switch (item.type) {
			case PIT_OK: {
				lisp_value_t evald = eval(item.as.value);
				value_decrefs(last_res);
				last_res = evald;
			} break;
			default: {
				fprintf(stderr, "Encountered error parsing file %s:\n", fname);
				fprintf(stderr, "%s:%lu,%lu %s\n", fname, item.pos.line, item.pos.pos - item.pos.line_start, item.as.error);
				for (size_t i = 0; i < item.pos.pos - item.pos.line_start; ++i) {
					fputc(' ', stderr);
				}
				for (const char *it = item.pos.pos; it < item.end.pos && *it != '\n'; ++it) {
					fputc('v', stderr);
				}
				fputc('\n', stderr);
				fprintf(stderr, "%.*s\n", (int)(item.end.pos - item.pos.line_start), item.pos.line_start);
				goto finish;
			} break;
		}
		parsed_item_destroy(&item);
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

		struct parser parser = parser_from_sb("<repl>", line_builder);

		struct parsed_item item = parser_next(&parser);
		switch (item.type) {
			case PIT_OK: {
				lisp_value_t evald = eval(item.as.value);
				value_decrefs(last_res);
				last_res = evald;
			} break;
			default: {
				fprintf(stderr, "Encountered error parsing line:\n");
				fprintf(stderr, "%s:%lu,%lu %s\n", "<repl>", item.pos.line, item.pos.pos - item.pos.line_start, item.as.error);
				for (size_t i = 0; i < item.pos.pos - item.pos.line_start; ++i) {
					fputc(' ', stderr);
				}
				for (const char *it = item.pos.pos; it < item.end.pos && *it != '\n'; ++it) {
					fputc('v', stderr);
				}
				fputc('\n', stderr);
				fprintf(stderr, "%.*s\n", (int)(item.end.pos - item.pos.line_start), item.pos.line_start);
			} break;
		}
		parsed_item_destroy(&item);

		struct string_builder sb = {0};
		value_repr(last_res, &sb);
		printf("%.*s\n", (int)sb.count, sb.items);
		sb_clear(&sb);
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
#define X(name, value) name ## _symbol = value_of_symbol(symbol_from_str(value));
	LIST_OF_PREDEFINED_SYMBOLS
#undef X

	for (size_t i = 0; i < sizeof(BUILTINS)/sizeof(*BUILTINS); ++i) {
		BUILTINS[i].fn.name = symbol_from_str(BUILTINS[i].name);
	}

	last_res = nil;

	da_append(&root_env, ((struct assoc) {
		.name = symbol_from_str("nil"),
		.value = nil,
	}));

	da_append(&root_env, ((struct assoc) {
		.name = symbol_from_str("stdin"),
		.value = file_to_lisp_val(stdin),
	}));
	da_append(&root_env, ((struct assoc) {
		.name = symbol_from_str("stdout"),
		.value = file_to_lisp_val(stdout),
	}));
	da_append(&root_env, ((struct assoc) {
		.name = symbol_from_str("stderr"),
		.value = file_to_lisp_val(stderr),
	}));

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
		args = list_append(args, (lisp_value_t) {
			.type = LT_STRING,
			.as.string = tmp,
		});
		string_decrefs(tmp);
	} else {
		args = list_append(args, nil);
	}
	for (int i = prog_args_start; i < argc; ++i) {
		string_t tmp = string_from_str(argv[i]);
		args = list_append(args, (lisp_value_t) {
			.type = LT_STRING,
			.as.string = tmp,
		});
		string_decrefs(tmp);
	}
	da_append(&root_env, ((struct assoc) {
		.name = symbol_from_str("args"),
		.value = value_of_list(args),
	}));

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

	value_decrefs(last_res);
}
