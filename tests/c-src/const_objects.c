struct interesting {
    void (*f1)();
    void (*f2)();
};

struct nested_const_ptr {
    const struct interesting *ptr;
    int dummy1;
    int dummy2;
};

struct nested_ptr {
    struct interesting *ptr;
    int dummy1;
    int dummy2;
};

struct nested_const_value {
    const struct interesting ptr;
    int dummy1;
    int dummy2;
};

void f1();
void f2();
void marker();

void fill_interesting(struct interesting* i) {
    i->f1 = marker;
    i->f2 = marker;
}

void external_consume(const struct interesting*);

void test_local_obj_flat() {
    struct interesting x = {
            .f1 = f1,
            .f2 = f2
    };
    external_consume(&x);
    x.f1();
    x.f2();
}