typedef void(*t1)(int, long);
typedef int(*t2)();

typedef void(*t3)();
typedef void(*t4)();

void t1_f1(int, long);
void t1_f2(int, long);
int t2_f1();
int t2_f2();
void t3_f1();
void t3_f2();
void t4_f1();
void t4_f2();

// Check that we track values passed out
// external_register1_arg_0 -> {t1_f1, pass_to_external_arg_0}
void external_register1(t1 f);
// external_register1_arg_0 -> {t3_f1, t3_f2}
void external_register2(t3 f, int x);

void call_externals1() {
    external_register1(t1_f1);
    external_register2(t3_f1, 0);
}

// Check how uncalled functions are resolved
// internal_caller1 -> {internal_caller1_arg_0, t2_f1, internal_caller1_arg_1, t3_f2}
void internal_caller1(t2 f, t3 g) {
    if (f() > 0) {
        g();
    }
}

void pass_to_external(t1 f) {
    external_register1(f);
    external_register2(t3_f2, 0);
    internal_caller1(t2_f1, t3_f2);
}