/*
 * Test value load and store with abstract typed pointers, that are not bound to any object
 * Also test abstract value store
 */
int f1_v1(int, void*);
int f1_v2(int, void*);
void f2_v1();
void f2_v2();

struct s1 {
	int (*f1)(int, void*);
	int x;
	void (*f2)();
};

void write_values1(struct s1 *v) {
	v->f1 = f1_v1;
	v->f2 = f2_v2;
}

void write_values2(struct s1 *v) {
	v->f1 = f1_v2;
	v->f2 = f2_v2;
}

void call_any(struct s1 *v) {
    // Expect f1_v1 and f1_v2
	v->f1(1, (void*)239);
    // Expect f2_v2
	v->f2();
}

struct s2 {
	int (*f1)(int, void*);
	int (*f2)(int, void*);
};

int test2_v1(int, void*);
int test2_v2(int, void*);
int test2_v3(int, void*);
int test2_v4(int, void*);

struct s2 s2_obj;

void fill_s2(struct s2* x) {
	x->f1 = test2_v1;
	x->f2 = test2_v4;	
}

void move_to_s2(struct s2* x) {
	s2_obj = *x;
}

void call_s2() {
    // Expect test2_v1
	s2_obj.f1(1, (void*)0);
    // Expect test2_v4
	s2_obj.f2(2, (void*)1);
}
