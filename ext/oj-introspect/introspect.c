#include <ruby.h>
#include "oj.h"
#include "parser.h"
#include "usual.h"

#define BYTE_OFFSETS_STACK_INC_SIZE 256

// Holds the start byte offsets of each JSON object encountered
struct _byte_offsets {
    int   length;
    int   current;
    long *stack;
};

typedef struct _introspect_S {
  struct _usual usual; // inherit all the attributes from `_usual` struct
  struct _byte_offsets byte_offsets; // I think it's better to encapsulate common fields under a namespace

  char *filter;
  bool introspect;

  void (*delegated_start_func)(struct _ojParser *p);
  void (*delegated_free_func)(struct _ojParser *p);
  VALUE (*delegated_option_func)(struct _ojParser *p, const char *key, VALUE value);
  void (*delegated_open_object_func)(struct _ojParser *p);
  void (*delegated_open_object_key_func)(struct _ojParser *p);
  void (*delegated_open_array_key_func)(struct _ojParser *p);
  void (*delegated_close_object_func)(struct _ojParser *p);
  void (*delegated_close_object_key_func)(struct _ojParser *p);
  void (*delegated_close_array_key_func)(struct _ojParser *p);
} * IntrospectDelegate;

static VALUE introspection_key;
static VALUE start_byte_key;
static VALUE end_byte_key;

static void dfree(ojParser p) {
  IntrospectDelegate d = (IntrospectDelegate)p->ctx;

  if(d->filter != NULL) xfree(d->filter);
  xfree(d->byte_offsets.stack);
  d->delegated_free_func(p);
}

static void start(ojParser p) {
    IntrospectDelegate d = (IntrospectDelegate)p->ctx;

    d->delegated_start_func(p);
    // Reset to zero so the parser and delegate can be reused.
    d->byte_offsets.current = 0;

    /*
    * If the `filter` is provided we will start introspecting later
    * once we encounter with the key provided.
    */
    d->introspect = (d->filter == NULL);
}

static void copy_ruby_str(char **target, VALUE source) {
  const char *ruby_str = StringValueCStr(source);
  size_t len = strlen(ruby_str);

  *target = ALLOC_N(char, len + 1);
  strncpy(*target, ruby_str, len + 1);
}

static VALUE option(ojParser p, const char *key, VALUE value) {
  IntrospectDelegate d = (IntrospectDelegate)p->ctx;

  if(strcmp(key, "filter=") == 0) {
    Check_Type(value, T_STRING);
    copy_ruby_str(&d->filter, value); // We need to copy the value as GC can free it later.

    return Qtrue;
  }

  return d->delegated_option_func(p, key, value);
}

static void ensure_byte_offsets_stack(IntrospectDelegate d) {
    if (RB_UNLIKELY(d->byte_offsets.current == (d->byte_offsets.length - 1))) {
        d->byte_offsets.length += BYTE_OFFSETS_STACK_INC_SIZE;
        REALLOC_N(d->byte_offsets.stack, long, d->byte_offsets.length + BYTE_OFFSETS_STACK_INC_SIZE);
    }
}

static long pop(ojParser p) {
    IntrospectDelegate d = (IntrospectDelegate)p->ctx;

    return d->byte_offsets.stack[--d->byte_offsets.current];
}

static void push(ojParser p) {
    IntrospectDelegate d = (IntrospectDelegate)p->ctx;
    ensure_byte_offsets_stack(d);

    d->byte_offsets.stack[d->byte_offsets.current++] = p->cur;
}

static void open_object_introspected(ojParser p) {
  push(p);

  IntrospectDelegate d = (IntrospectDelegate)p->ctx;
  d->delegated_open_object_func(p);
}

static char * previously_inserted_key(IntrospectDelegate d) {
  Key key = (d->usual.ktail - 1);

  return ((size_t)key->len < sizeof(key->buf)) ? key->buf : key->key;
}

static bool should_switch_introspection(IntrospectDelegate d) {
  return strcmp(d->filter, previously_inserted_key(d)) == 0;
}

/*
* WHEN there is a filter
* AND
*     WHEN the introspection is disabled
*     AND the last inserted key matches the filter
*     THEN enable introspection
*   OR
*     WHEN the introspection is enabled
*     AND the last inserted key matches the filter
*     THEN disable introspection
*/
static void switch_introspection(IntrospectDelegate d) {
  if(d->filter == NULL) return;

  d->introspect = should_switch_introspection(d) != d->introspect; // a XOR b
}

static void open_object_key_introspected(ojParser p) {
  push(p);

  IntrospectDelegate d = (IntrospectDelegate)p->ctx;
  d->delegated_open_object_key_func(p);

  if(!d->introspect) switch_introspection(d);
}

static void open_array_key_introspected(ojParser p) {
  IntrospectDelegate d = (IntrospectDelegate)p->ctx;
  d->delegated_open_array_key_func(p);

  if(!d->introspect) switch_introspection(d);
}

static void set_introspection_values(ojParser p) {
    IntrospectDelegate d = (IntrospectDelegate)p->ctx;

    if(!d->introspect) return;

    volatile VALUE obj = rb_hash_new();
    rb_hash_aset(obj, start_byte_key, INT2FIX(pop(p)));
    rb_hash_aset(obj, end_byte_key, INT2FIX(p->cur));
    rb_hash_aset(*(d->usual.vtail - 1), introspection_key, obj);
}

static void close_object_introspected(ojParser p) {
  IntrospectDelegate d = (IntrospectDelegate)p->ctx;
  d->delegated_close_object_func(p);

  set_introspection_values(p);
}

// We switch introspection off only for object and array keys.
static void close_object_key_introspected(ojParser p) {
  IntrospectDelegate d = (IntrospectDelegate)p->ctx;
  d->delegated_close_object_func(p);

  if(d->introspect) {
    set_introspection_values(p);
    switch_introspection(d);
  }
}

static void close_array_key_introspected(ojParser p) {
  IntrospectDelegate d = (IntrospectDelegate)p->ctx;
  d->delegated_close_array_key_func(p);

  if(d->introspect) switch_introspection(d);
}

static void init_introspect_parser(ojParser p, VALUE ropts) {
  IntrospectDelegate d = ALLOC(struct _introspect_S);

  oj_init_usual(p, &d->usual);

  // now function mangling...
  d->delegated_free_func = p->free;
  p->free = &dfree;

  d->delegated_start_func = p->start;
  p->start = start;

  d->delegated_option_func = p->option;
  p->option = option;

  // Wrap original functions to collect byte offsets
  Funcs f = &p->funcs[TOP_FUN];
  d->delegated_open_object_func = f->open_object;
  d->delegated_close_object_func = f->close_object;
  f->open_object  = open_object_introspected;
  f->close_object = close_object_introspected;

  f = &p->funcs[ARRAY_FUN];
  f->open_object  = open_object_introspected;
  f->close_object = close_object_introspected;

  f = &p->funcs[OBJECT_FUN];
  d->delegated_open_array_key_func = f->open_array;
  d->delegated_open_object_key_func = f->open_object;
  d->delegated_close_array_key_func = f->close_array;
  d->delegated_close_object_key_func = f->close_object;
  f->open_array = open_array_key_introspected;
  f->open_object  = open_object_key_introspected;
  f->close_array = close_array_key_introspected;
  f->close_object = close_object_key_introspected;

  // Init stack
  d->byte_offsets.current = 0;
  d->byte_offsets.stack   = ALLOC_N(long, BYTE_OFFSETS_STACK_INC_SIZE);
  d->byte_offsets.length  = BYTE_OFFSETS_STACK_INC_SIZE;

  d->filter = NULL;

  // Process options.
  oj_parser_set_option(p, ropts);
}

static VALUE rb_new_introspect_parser(int argc, VALUE *argv, VALUE self) {
  rb_check_arity(argc, 0, 1);

  VALUE options;

  if(argc == 1) {
    options = argv[0];

    Check_Type(options, T_HASH);
  } else {
    options = rb_hash_new();
  }


  VALUE oj_parser = oj_parser_new();
  struct _ojParser *p;
  Data_Get_Struct(oj_parser, struct _ojParser, p);

  init_introspect_parser(p, options);

  return oj_parser;
}

// This code is neither Ruby Thread safe nor Ractor safe!
// I don't really want to write a mutex right now.
static VALUE rb_get_default_introspect_parser(VALUE self) {
  VALUE current_thread = rb_funcall(rb_cThread, rb_intern("current"), 0);
  VALUE thread_parser = rb_funcall(current_thread, rb_intern("[]"), 1, rb_str_new_literal("oj_introspect_parser"));

  if(RTEST(thread_parser))
    return thread_parser;

  thread_parser = rb_new_introspect_parser(0, NULL, self);
  rb_funcall(current_thread, rb_intern("[]="), 2, rb_str_new_literal("oj_introspect_parser"), thread_parser);

  return thread_parser;
}

void Init_introspect_ext() {
  VALUE oj_module = rb_const_get(rb_cObject, rb_intern("Oj"));
  VALUE parser_module = rb_const_get(oj_module, rb_intern("Parser"));
  VALUE introspection_class = rb_define_class_under(oj_module, "Introspect", rb_cObject);

  introspection_key = ID2SYM(rb_intern("__oj_introspection"));
  rb_gc_register_address(&introspection_key);
  start_byte_key = ID2SYM(rb_intern("start_byte"));
  rb_gc_register_address(&start_byte_key);
  end_byte_key = ID2SYM(rb_intern("end_byte"));
  rb_gc_register_address(&end_byte_key);

  rb_const_set(introspection_class, rb_intern("KEY"), introspection_key);
  rb_define_singleton_method(parser_module, "introspect", rb_get_default_introspect_parser, 0);
  rb_define_singleton_method(introspection_class, "new", rb_new_introspect_parser, -1);
}
