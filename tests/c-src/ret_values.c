/*
 * Test value collection for functions, which return interesting values and are not called
 */
#include <stdlib.h>

struct s {
	void (*f1)();
	void (*f2)();
	int dummy;
};

void f1();
void f2();
void f3();
void f4();

struct s get_value() {
	struct s x;
	x.f1 = f1;
	x.f2 = f2;
	return x;
}

struct s* get_ptr_value() {
	struct s* x = malloc(sizeof(struct s));
	x->f1 = f3;
	x->f2 = f1;
	return x;
}
