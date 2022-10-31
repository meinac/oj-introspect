#include <ruby.h>
#include "oj.h"
#include "parser.h"
#include "usual.h"

#define BYTE_OFFSETS_STACK_INC_SIZE 256

// Holds the start byte offsets of each JSON object encountered
typedef struct _byte_offsets {
    int   length;
    int   current;
    long *stack;
} ByteOffsets;

typedef struct _introspect_S {
  struct _usual usual; // inherit all the attributes from `_usual` struct

  ByteOffsets *byte_offsets;

  void (*delegated_free_func)(struct _ojParser *p);
  void (*delegated_open_object_func)(struct _ojParser *p);
  void (*delegated_open_object_key_func)(struct _ojParser *p);
  void (*delegated_close_object_func)(struct _ojParser *p);
} *IntrospectDelegate;

static void dfree(ojParser p) {
  IntrospectDelegate d = (IntrospectDelegate)p->ctx;

  xfree(d->byte_offsets->stack);
  xfree(d->byte_offsets);
  d->delegated_free_func(p);
}

static void ensure_byte_offsets_stack(IntrospectDelegate d) {
    if (RB_UNLIKELY(d->byte_offsets->current == (d->byte_offsets->length - 1))) {
        d->byte_offsets->length += BYTE_OFFSETS_STACK_INC_SIZE;
        REALLOC_N(d->byte_offsets->stack, long, d->byte_offsets->length + BYTE_OFFSETS_STACK_INC_SIZE);
    }
}

static long pop(ojParser p) {
    IntrospectDelegate d = (IntrospectDelegate)p->ctx;

    return d->byte_offsets->stack[--d->byte_offsets->current];
}

static void push(ojParser p) {
    IntrospectDelegate d = (IntrospectDelegate)p->ctx;
    ensure_byte_offsets_stack(d);

    d->byte_offsets->stack[d->byte_offsets->current++] = p->cur;
}

static void open_object_introspected(ojParser p) {
  push(p);

  IntrospectDelegate d = (IntrospectDelegate)p->ctx;
  d->delegated_open_object_func(p);
}

static void open_object_key_introspected(ojParser p) {
  push(p);

  IntrospectDelegate d = (IntrospectDelegate)p->ctx;
  d->delegated_open_object_key_func(p);
}

static void set_introspection_values(ojParser p) {
    IntrospectDelegate d = (IntrospectDelegate)p->ctx;

    volatile VALUE obj = rb_hash_new();
    rb_hash_aset(obj, ID2SYM(rb_intern("start_byte")), INT2FIX(pop(p)));
    rb_hash_aset(obj, ID2SYM(rb_intern("end_byte")), INT2FIX(p->cur));
    rb_hash_aset(*(d->usual.vtail - 1), ID2SYM(rb_intern("__oj_introspection")), obj);
}

static void close_object_introspected(ojParser p) {
  IntrospectDelegate d = (IntrospectDelegate)p->ctx;
  d->delegated_close_object_func(p);

  set_introspection_values(p);
}

static void init_introspect_parser(ojParser p) {
  IntrospectDelegate d = ALLOC(struct _introspect_S);

  oj_init_usual(p, (Usual)d);

  // now function mangling...
  d->delegated_free_func = p->free;
  p->free = &dfree;

  Funcs f = &p->funcs[TOP_FUN];
  d->delegated_open_object_func = f->open_object;
  d->delegated_close_object_func = f->close_object;
  f->open_object  = open_object_introspected;
  f->close_object = close_object_introspected;

  f = &p->funcs[ARRAY_FUN];
  f->open_object  = open_object_introspected;
  f->close_object = close_object_introspected;

  f = &p->funcs[OBJECT_FUN];
  d->delegated_open_object_key_func = f->open_object;
  f->open_object  = open_object_key_introspected;
  f->close_object = close_object_introspected;

  // Init stack
  d->byte_offsets          = ALLOC(ByteOffsets);
  d->byte_offsets->current = 0;
  d->byte_offsets->stack   = ALLOC_N(long, BYTE_OFFSETS_STACK_INC_SIZE);
  d->byte_offsets->length  = BYTE_OFFSETS_STACK_INC_SIZE;
}

VALUE oj_get_parser_introspect() {
  VALUE oj_parser = oj_parser_new();
  struct _ojParser *p;
  Data_Get_Struct(oj_parser, struct _ojParser, p);

  init_introspect_parser(p);

  rb_gc_register_address(&oj_parser);

  return oj_parser;
}

static VALUE rb_new_introspect_parser() {
  VALUE oj_parser = oj_parser_new();
  struct _ojParser *p;
  Data_Get_Struct(oj_parser, struct _ojParser, p);

  init_introspect_parser(p);

  rb_gc_register_address(&oj_parser);

  return oj_parser;
}

// This code is neither Ruby Thread safe nor Ractor safe!
// I don't really want to write a mutex right now.
static VALUE rb_get_default_introspect_parser() {
  VALUE current_thread = rb_funcall(rb_cThread, rb_intern("current"), 0);
  VALUE thread_parser = rb_funcall(current_thread, rb_intern("[]"), 1, rb_str_new_literal("oj_introspect_parser"));

  if(RTEST(thread_parser))
    return thread_parser;

  thread_parser = rb_new_introspect_parser();
  rb_funcall(current_thread, rb_intern("[]="), 2, rb_str_new_literal("oj_introspect_parser"), thread_parser);

  return thread_parser;
}

void Init_introspect_ext() {
  VALUE oj_module = rb_const_get(rb_cObject, rb_intern("Oj"));
  VALUE parser_module = rb_const_get(oj_module, rb_intern("Parser"));
  VALUE introspection_class = rb_define_class_under(oj_module, "Introspect", rb_cObject);

  rb_define_singleton_method(parser_module, "introspect", rb_get_default_introspect_parser, 0);
  rb_define_singleton_method(introspection_class, "new", rb_new_introspect_parser, 0);
}
