/*
 * Test struct with a pointer to interesting object
 */
struct inner_t {
	void (*f)();
	int dummy1;
	int dummy2;
};

struct nested_value {
	int dummy1;
	struct inner_t *x;
	int dummy2;
};

void f1();
void f2();

void fill_nested_value(struct nested_value *v) {
	v->x->f = f1;
}

void fill_inner_t(struct inner_t *v) {
	v->f = f2;
}

void call_nested_value(struct nested_value *v) {
    // Expect f1 and f2
	v->x->f();
}

void call_inner_t(struct inner_t *v) {
    // Expect f1 and f2
    v->f();
}

