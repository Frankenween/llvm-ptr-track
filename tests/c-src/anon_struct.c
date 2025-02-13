struct nested_value {
    struct {
        void (*f1)();
        void (*f2)();
        int x;
        int y;
    };
    int dummy;
};

void f1();
void f2();

void fill_nested_value(struct nested_value *v) {
    v->f1 = f1;
}

struct nested_value fill_inner_t() {
    struct nested_value v;
    v.f2 = f2;
    return v;
}

void call_nested_value(struct nested_value *v) {
    // Expect f1 and f2
    // We can get a reference of nested_value.x, so f2 could be written
    v->f1();
    v->f2();
}

void call_inner_t(struct nested_value v) {
    // Expect f1 and f2
    // We can call this function with &nested_value.x
    v.f1();
    v.f2();
}

