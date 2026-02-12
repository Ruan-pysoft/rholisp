#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

typedef int64_t i64;

#define MAX_LINE_LEN (1 << 8)
char buf[MAX_LINE_LEN];
size_t line_len;
char *line;

enum input_status {
	IS_SUCCESS,
	IS_LINE_TOO_LONG,
	IS_EOF,
};
enum input_status input(const char *prompt, char *buf, size_t buf_len, FILE *infile) {
	fputs(prompt, stdout);
	for (size_t i = 0; i < buf_len; ++i) {
		const int inp = getc(infile);
		if (inp == EOF) return IS_EOF;
		if (inp == '\n') {
			buf[i] = '\0';
			line = buf;
			line_len = i;
			return IS_SUCCESS;
		}
		buf[i] = inp;
	}
	return IS_LINE_TOO_LONG;
}

enum lisp_type {
	LT_NUM,
	LT_BUILTIN,
	LT_SYM,
	LT_LIST,
	LT_BOOL,
	LT_STRING,
};
struct lisp_val;
struct list;
typedef struct list *list_t;
struct string;
typedef struct string *string_t;
struct sym;
typedef struct sym *sym_t;

struct call_res;
typedef struct call_res (*lfn)(list_t args);
enum builtin_type {
	BT_FN,
	BT_MACRO,
	BT_TRANSFORM,
};
struct lbuiltin {
	lfn fn;
	bool eval_args;
	sym_t name;
	const char *doc;
};
struct call_res lb_call(struct lbuiltin fn, list_t args, bool pre_evald);
struct call_res ll_call(list_t fn, list_t args, bool tailcall, bool pre_evald);

struct sym {
	const char *sym;
	size_t refcount;
};
sym_t sym_from_strn(const char *str, size_t len) {
	assert(str != NULL);

	sym_t res = malloc(sizeof(struct sym));
	*res = (struct sym) {
		.sym = strndup(str, len),
		.refcount = 1,
	};
	return res;
}
sym_t sym_from_str(const char *str) {
	return sym_from_strn(str, strlen(str));
}
sym_t sym_copy(sym_t sym) {
	assert(sym != NULL);
	assert(sym->refcount != 0);

	++sym->refcount;
	return sym;
}
void sym_free(sym_t sym) {
	assert(sym != NULL);
	if (sym->refcount == 0) {
		fprintf(stderr, "error: re-freeing `%s`!\n", sym->sym);
	}
	assert(sym->refcount != 0);

	--sym->refcount;
	if (!sym->refcount) {
		// fprintf(stderr, "freeing `%s`...\n", sym->sym);

		free((void*)sym->sym);
		free(sym);
	}
}

struct string {
	char *data;
	size_t len;

	string_t borrows;
	size_t refcount;
};
string_t string_from_strn(const char *str, size_t len) {
	assert(str != NULL);

	string_t res = malloc(sizeof(struct string));
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
string_t string_copy(string_t string) {
	assert(string != NULL);
	assert(string->refcount != 0);

	// fprintf(stderr, "string %p's refs: %lu", string, string->refcount);
	++string->refcount;
	// fprintf(stderr, " -> %lu\n", string->refcount);
	return string;
}
void string_free(string_t string) {
	assert(string != NULL);
	if (string->refcount == 0) {
		fprintf(stderr, "error: re-freeing \"%.*s\"!\n", (int)string->len, string->data);
	}
	assert(string->refcount != 0);

	// fprintf(stderr, "string %p's refs: %lu", string, string->refcount);
	--string->refcount;
	// fprintf(stderr, " -> %lu\n", string->refcount);
	if (!string->refcount) {
		// fprintf(stderr, "freeing \"%.*s\"...\n", (int)string->len, string->data);

		if (!string->borrows) free((void*)string->data);
		else string_free(string->borrows);
		free(string);
	}
}
string_t string_substr(string_t string, size_t begin, size_t end) {
	assert(string != NULL);
	assert(begin <= end);
	assert(end <= string->len);

	string_t res = malloc(sizeof(struct string));
	*res = (struct string) {
		.data = &string->data[begin],
		.len = end - begin,
		.borrows = string_copy(string),
		.refcount = 1,
	};
	// fprintf(stderr, "after substr, string %p has %lu refs\n", string, string->refcount);
	return res;
}

struct lisp_val {
	enum lisp_type type;
	union {
		i64 num;
		struct lbuiltin builtin;
		sym_t sym;
		list_t list;
		bool boo;
		string_t string;
	} as;
};
list_t list_copy(list_t list);
struct lisp_val lisp_val_copy(struct lisp_val val) {
	if (val.type == LT_SYM) sym_copy(val.as.sym);
	else if (val.type == LT_LIST) list_copy(val.as.list);
	else if (val.type == LT_STRING) string_copy(val.as.string);

	return val;
}
void list_free(list_t list);
void lisp_val_print(struct lisp_val this, FILE *f);
void lisp_val_free(struct lisp_val val) {
	// fputs("freeing val ", stderr);
	// lisp_val_print(val, stderr);
	// fprintf(stderr, " of type %d\n", val.type);

	if (val.type == LT_SYM) sym_free(val.as.sym);
	else if (val.type == LT_LIST) list_free(val.as.list);
	else if (val.type == LT_STRING) string_free(val.as.string);
}

struct call_res {
	struct lisp_val val;
	bool destroy_env;
	bool eval;
};

struct list {
	struct lisp_val val;
	list_t next;

	size_t refcount;
};

void list_print(list_t this, FILE *f);
list_t list_copy(list_t list) {
	// fputs("copying list ", stderr);
	// list_print(list, stderr);
	// fprintf(stderr, " with %lu refs...\n", list ? list->refcount : 0);

	if (list == NULL) return NULL;
	assert(list->refcount != 0);

	++list->refcount;
	return list;
}
void list_free(list_t list) {
	// fputs("freeing list ", stderr);
	// list_print(list, stderr);
	// fprintf(stderr, " with %lu refs...\n", list ? list->refcount : 0);

	if (list == NULL) return;
	if (list->refcount == 0) {
		fprintf(stderr, "error: re-freeing (%p)!\n", list);
	}
	assert(list->refcount != 0);

	--list->refcount;
	if (!list->refcount) {
		// fprintf(stderr, "freeing (%p)...\n", list);

		list_free(list->next);
		lisp_val_free(list->val);
		free(list);
	}
}

const struct lisp_val nil = {
	.type = LT_LIST,
	.as.list = NULL,
};

list_t list_cons(struct lisp_val head, list_t tail) {
	list_t res = malloc(sizeof(*res));
	*res = (struct list) {
		.val = lisp_val_copy(head),
		.next = list_copy(tail),
		.refcount = 1,
	};
	return res;
}
list_t list_append(list_t this, struct lisp_val val) {
	// WARNING: unsafe operation:
	// modifies an existing list
	// (values should be const)
	list_t tail = malloc(sizeof(*tail));
	*tail = (struct list) {
		.val = lisp_val_copy(val),
		.next = NULL,
		.refcount = 1,
	};

	if (this == NULL) {
		return tail;
	}

	list_t pre_tail = this;
	while (pre_tail->next != NULL) {
		pre_tail = pre_tail->next;
	}
	pre_tail->next = tail;

	return this;
}
list_t list_dup(list_t this) {
	list_t res = NULL;

	while (this != NULL) {
		res = list_append(res, this->val);

		this = this->next;
	}

	return res;
}

void lisp_val_print(struct lisp_val this, FILE *f);
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
void string_print(string_t this, FILE *f) {
	fputc('"', f);
	for (size_t i = 0; i < this->len; ++i) {
		for (size_t j = 0; j < sizeof(escapes)/sizeof(*escapes); ++j) {
			if (escapes[j][0] == this->data[i]) {
				fputc('\\', f);
				fputc(escapes[j][1], f);
				goto escaped;
			}
		}
		fputc(this->data[i], f);
escaped:;
	}
	fputc('"', f);
}
void list_print(list_t this, FILE *f) {
	fputc('(', f);
	while (this != NULL) {
		lisp_val_print(this->val, f);
		if (this->next != NULL) {
			fputc(' ', f);
		}
		this = this->next;
	}
	fputc(')', f);
}

void lisp_val_print(struct lisp_val this, FILE *f) {
	switch (this.type) {
		case LT_NUM: {
			fprintf(f, "%ld", this.as.num);
		} break;
		case LT_BUILTIN: {
			fputs(this.as.builtin.eval_args ? "<builtin function>" : "<builtin macro>", f);
		} break;
		case LT_SYM: {
			fputs(this.as.sym->sym, f);
		} break;
		case LT_LIST: {
			list_print(this.as.list, f);
		} break;
		case LT_BOOL: {
			fputc(this.as.boo ? 'T' : 'F', f);
		} break;
		case LT_STRING: {
			string_print(this.as.string, f);
		} break;
	}
}

struct string_builder {
	char *items;
	size_t count;
	size_t capacity;
};
void sb_addb(struct string_builder *sb, const char *b, size_t l) {
	if (sb->capacity == 0) {
		assert(sb->items == NULL);

		sb->items = malloc(16);
		sb->count = 0;
		sb->capacity = 16;
	}

	while (l + sb->count > sb->capacity) {
		sb->capacity *= 2;
		sb->items = realloc(sb->items, sb->capacity);
	}

	if (l == 0) return;
	if (l == 1) {
		sb->items[sb->count++] = *b;
		return;
	}

	memcpy(&sb->items[sb->count], b, l);
	sb->count += l;
}
void sb_adds(struct string_builder *sb, const char *s) {
	sb_addb(sb, s, strlen(s));
}
void sb_addc(struct string_builder *sb, char c) {
	sb_addb(sb, &c, 1);
}
void sb_clear(struct string_builder *sb) {
	if (sb->items) free(sb->items);
	*sb = (struct string_builder) {0};
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
	sb_clear(sb);

	return res;
}

void lisp_val_repr(struct lisp_val this, struct string_builder *sb);
void string_repr(string_t this, struct string_builder *sb) {
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
void list_repr(list_t this, struct string_builder *sb) {
	sb_addc(sb, '(');
	while (this != NULL) {
		lisp_val_repr(this->val, sb);
		if (this->next != NULL) {
			sb_addc(sb, ' ');
		}
		this = this->next;
	}
	sb_addc(sb, ')');
}

void num_repr(i64 this, struct string_builder *sb) {
	uint64_t uthis = *(uint64_t*)&this;
	if (this < 0) {
		sb_addc(sb, '-');
		uthis = 1+~uthis;
	}
	if (this == 0) {
		sb_addc(sb, '0');
		return;
	}
	const size_t begin = sb->count;
	while (uthis) {
		sb_addc(sb, '0'+(uthis%10));
		uthis /= 10;
	}
	const size_t len = sb->count - begin;

	for (size_t i = 0; i < len/2; ++i) {
		const char tmp = sb->items[begin + i];
		sb->items[begin + i] = sb->items[begin+len-1 - i];
		sb->items[begin+len-1 - i] = tmp;
	}
}

void lisp_val_repr(struct lisp_val this, struct string_builder *sb) {
	switch (this.type) {
		case LT_NUM: {
			num_repr(this.as.num, sb);
		} break;
		case LT_BUILTIN: {
			sb_adds(sb, this.as.builtin.eval_args ? "<builtin function>" : "<builtin macro>");
		} break;
		case LT_SYM: {
			sb_adds(sb, this.as.sym->sym);
		} break;
		case LT_LIST: {
			list_repr(this.as.list, sb);
		} break;
		case LT_BOOL: {
			sb_addc(sb, this.as.boo ? 'T' : 'F');
		} break;
		case LT_STRING: {
			string_repr(this.as.string, sb);
		} break;
	}
}

bool lisp_val_is_truthy(struct lisp_val this) {
	switch (this.type) {
		case LT_NUM: {
			return this.as.num != 0;
		} break;
		case LT_LIST: {
			return this.as.list != NULL;
		} break;
		case LT_BOOL: {
			return this.as.boo;
		} break;
		case LT_STRING: {
			return this.as.string->len != 0;
		} break;
		default: return true;
	}

	assert(false && "unreachable");
}

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
		// fputs("after appending ", stderr);
		// lisp_val_print(tmp, stderr);
		// fputs(" to the list, we have: ", stderr);
		// list_print(res, stderr);
		// fprintf(stderr, "; ref(list) = %lu\n", res->refcount);
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

struct assoc {
	sym_t name;
	struct lisp_val value;
};
struct env {
	struct env *parent;
	bool fixed;
	list_t params_of;

	struct assoc *items;
	size_t count;
	size_t capacity;
};
#define env_for_each(env, it) for (struct assoc *it = (env)->items; it < (env)->items + (env)->count; ++it)
void env_add(struct env *env, struct assoc item) {
	assert(env != NULL);

	if (env->items == NULL) {
		env->items = malloc(sizeof(*env->items)*64);
		env->count = 0;
		env->capacity = 64;
	}

	if (env->count == env->capacity) {
		env->capacity *= 2;
		env->items = realloc(env->items, sizeof(*env->items)*env->capacity);
	}

	env->items[env->count++] = item;
}
void env_def(struct env *env, sym_t name, struct lisp_val value) {
	env_add(env, (struct assoc) { sym_copy(name), lisp_val_copy(value) });
}
struct assoc *find_var(struct env *env, sym_t name) {
	if (env == NULL) {
		return NULL;
	}

	size_t i = env->count;
	while (i --> 0) {
		if (strcmp(env->items[i].name->sym, name->sym) == 0) return &env->items[i];
	}

	return find_var(env->parent, name);
}
void env_clear(struct env *env) {
	assert(env != NULL);

	if (env->params_of) {
		list_free(env->params_of);
		env->params_of = NULL;
	}

	env_for_each(env, it) {
		sym_free(it->name);
		lisp_val_free(it->value);
	}
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

struct sym _nil_sym = { .sym = "nil", .refcount = 1 };
struct env root_env = {0};
struct env *curr_env = &root_env;

#define lret(_val, ...) return (struct call_res) { .val = _val, .eval = false, .destroy_env = false, __VA_ARGS__ }

struct call_res ladd(list_t args) {
	assert(args != NULL);

	// fputs("args: ", stdout);
	// list_print(args, stdout);
	// putchar('\n');

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
	assert(args->val.type == LT_STRING | args->val.type == LT_NUM);
	assert(args->next == NULL);

	if (args->val.type == LT_STRING) {
		printf("%.*s", (int)args->val.as.string->len, args->val.as.string->data);
	} else {
		assert(0 <= args->val.as.num && args->val.as.num < 256);

		putchar(args->val.as.num);
	}

	lret(nil);

	/*struct lisp_val res = lisp_val_copy(nil);
	bool first = true;

	while (args != NULL) {
		if (!first) putchar(' ');
		first = false;
		if (args->val.type == LT_STRING) {
			printf("%.*s", (int)args->val.as.string->len, args->val.as.string->data);
		} else {
			lisp_val_print(args->val, stdout);
		}
		lisp_val_free(res);
		res = lisp_val_copy(args->val);
		args = args->next;
	}
	putchar('\n');

	lret(res);*/
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

			// fprintf(stderr, "comparing \"%.*s\" and \"%.*s\"...\n", sa->len, sa->data, sb->len, sb->data);

			const size_t upto = sa->len < sb->len ? sa->len : sb->len;
			for (size_t i = 0; i < upto; ++i) {
				if (sa->data[i] < sb->data[i]) return -1;
				else if (sa->data[i] > sb->data[i]) return 1;
			}
			return sa->len == sb->len ? 0 : sa->len < sb->len ? -1 : 1;
		}
	}
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

	// fprintf(stderr, "running or...\n");

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
}

/*struct call_res lprompt(list_t args) {
	while (args != NULL) {
		lisp_val_print(args->val, stdout);
		putchar(' ');
		args = args->next;
	}

	enum input_status input_status;
	input_status = input("", buf, MAX_LINE_LEN, stdin);
	if (input_status == IS_EOF) {
		lret(lisp_val_copy(nil));
	} else if (input_status == IS_LINE_TOO_LONG) {
		printf("ERROR: Longest supported line is %d characters\n", MAX_LINE_LEN);
		lret(lisp_val_copy(nil));
	}

	line = buf;
	line_len = strlen(line);

	// cast &line to (void*) to side-step incorrect constness warnings from the compiler
	lret(lisp_val_parse((void*)&line, &line_len));
}*/

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
	// fputs("getting remainder...\n", stderr);
	string_t tmp = string_substr(args->val.as.string, str - args->val.as.string->data, args->val.as.string->len);
	res = list_append(res, (struct lisp_val) {
		.type = LT_STRING,
		.as.string = tmp,
	});
	// fputs("freeing remainder...\n", stderr);
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

	bool eof = false;

	for (;;) {
		const int inp = getc(f);
		if (inp == EOF) {
			eof = true;
			break;
		}
		if (inp == '\n') {
			break;
		}
		sb_addc(&sb, inp);
	}

	if (sb.count == 0 && eof) {
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
	// or maybe? (I thought I hadn't implemented TCO for (call), but it seems I have?)
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
	// { "prompt", { lprompt, true, NULL, NULL } },
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
		// fputs("is macro!\n", stderr);

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

		// fputs("pre-sub:  ", stderr);
		// lisp_val_print(body, stderr);
		// fputc('\n', stderr);
		struct lisp_val body = substitute(lfn.body, &env);
		// fputs("post-sub: ", stderr);
		// lisp_val_print(body, stderr);
		// fputc('\n', stderr);

		// fputs("got substituted body: ", stderr);
		// lisp_val_print(body, stderr);
		// fputc('\n', stderr);

		env_clear(&env);
		list_fn_free(lfn);

		return (struct call_res) {
			.val = body,
			.eval = true,
			.destroy_env = false,
		};
	} else {
		// fputs("is not macro!\n", stderr);

		bool replace_env = tailcall && curr_env->params_of == fn;
		// fprintf(stderr, "replacing env? %d\n", replace_env);

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
		input_status = input("> ", buf, MAX_LINE_LEN, stdin);
		if (input_status == IS_EOF) {
			break;
		} else if (input_status == IS_LINE_TOO_LONG) {
			printf("ERROR: Longest supported line is %d characters\n", MAX_LINE_LEN);
			continue;
		}

		line = buf;
		line_len = strlen(line);

		// cast &line to (void*) to side-step incorrect constness warnings from the compiler
		struct lisp_val parsed = lisp_val_parse((void*)&line, &line_len);
		// fputs("parsed value: ", stderr);
		// lisp_val_print(parsed, stderr);
		// fputc('\n', stderr);
		struct lisp_val evald = eval(parsed);
		// fputs("evald value: ", stderr);
		// lisp_val_print(evald, stderr);
		// fputc('\n', stderr);

		// fputs("freeing parsed value...\n", stderr);
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
		// run_file("/data/data/com.termux/files/home/lisp/std.lisp");
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

	// fputs("freeing last result: ", stderr);
	// lisp_val_print(last_res, stderr);
	// fputc('\n', stderr);
	lisp_val_free(last_res);
}
