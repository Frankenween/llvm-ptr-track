/*
 * Test that there is no interference between generic object pointers and objects with no public access
 */
struct s1 {
	int (*f1)(int, void*);
	void (*f2)();
	int dummy;
};

int f1_v1(int, void*);
int f1_v2(int, void*);
int f1_v3(int, void*);
int f1_v4(int, void*);

void f2_v1();
void f2_v2();
void f2_v3();
void f2_v4();

struct s1 obj1;

// Function is static, so do not propagate any values in it
static void fill_obj1(struct s1 arg1, int a, char b, void* c, struct s1 arg2) {
	if (c || a + b < 10) {
		obj1 = arg1;
	} else {
		obj1 = arg2;
	}
}

void write1() {
	struct s1 x;
	x.f1 = f1_v1;
	x.f2 = f2_v1;
	struct s1 y;
	y.f1 = f1_v2;
	y.f2 = f2_v1;

	fill_obj1(x, 1, 'a', (void*)0xDEAD, y);
}

void do_calls_provided(struct s1 x) {
    // Expect f1_v3
    // TODO: add f1_v1 and f1_v2
	x.f1(0, (void*)1);
    // Expect f2_v4
    // TODO: add f2_v1
	x.f2();
}

void do_calls_obj() {
    // Expect f1_v1, f1_v2
	obj1.f1(1, (void*)2);
    // Expect f2_v1
	obj1.f2();
}

void fill_by_ptr(struct s1 *x) {
	x->f1 = f1_v3;
	x->f2 = f2_v4;
}

