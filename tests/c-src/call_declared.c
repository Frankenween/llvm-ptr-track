/*
 * Test value tracking for declared functions
 */

struct s {
	void (*f1)();
	void (*f2)();
	int dummy;
};

void a();
void b();
void c();
void d();

void external_call(struct s*);
void external_value_consumer(struct s);

struct s get_outside1();
struct s* get_outside2();

void register_struct() {
	struct s val;
	val.f1 = b;
	val.f2 = c;
	external_call(&val);
}

void register_struct_ptr() {
	struct s val;
	val.f2 = d;
	external_value_consumer(val);
}
void pass_ptr_as_val() {
	struct s* x = get_outside2();
	x->f1 = a;
	external_value_consumer(*x);
}

void test_get_extern1() {
	struct s x = get_outside1();
    // Expect a and b here
	x.f1();
}

void test_get_extern2() {
	struct s *x = get_outside2();
    // Expect c and d here
	x->f2();
}
