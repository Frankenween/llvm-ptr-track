/*
 * Test registration of structure with nested interesting object
 */

struct interesting {
    void (*f1)();
    void (*f2)();
    int dummy;
};

struct nested_ptr {
    int dummy1;
    struct interesting* ptr;
    int dummy2;
};

struct nested_value {
    int dummy1;
    struct interesting val;
    int dummy2;
};

void f1();
void f2();
void f3();
void f4();

void consume_nested_val(struct nested_value*);
void consume_nested_ptr(struct nested_ptr*);

struct interesting i1 = {
    .f1 = f1,
    .f2 = f2
};

struct nested_ptr n1 = {
    .ptr = &i1
};

struct nested_value n2 = {
        .val = {
                .f1 = f3,
                .f2 = f4
        }
};

void test_ptr() {
    consume_nested_ptr(&n1);
}

void test_val() {
    consume_nested_val(&n2);
}
