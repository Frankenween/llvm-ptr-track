struct inner1 {
    int x;
    void (*f)();
    int y;
};

struct outer1 {
    int x;
    struct inner1 in;
    int y;
    int (*g)(void*);
};

struct inner2 {
    int x;
    int (*f)(long);
    int y;
};

struct outer2 {
    int x;
    struct inner2 *in;
    int y;
    long (*g)(int);
};

void inner1_f1();
void inner1_f2();

int outer1_g1(void*);
int outer1_g2(void*);

int inner2_f1(long);
int inner2_f2(long);

long outer2_g1(int);
long outer2_g2(int);

struct outer1 arr1[] = {
        {
            .in = {
                    .f = inner1_f1
            },
            .g = outer1_g1,
        },
        {},
        {
                .in = {
                        .f = inner1_f2
                },
                .g = outer1_g2,
        },
};

struct inner2 in2_1 = {
        .f = inner2_f1
};

struct inner2 in2_2 = {
        .f = inner2_f2
};

struct outer2 arr2[] = {
        {
                .in = &in2_1,
                .g = outer2_g1,
        },
        {
                .in = &in2_2,
                .g = outer2_g2,
        },
};
