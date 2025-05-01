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

// https://elixir.bootlin.com/linux/v5.15/source/fs/fuse/virtio_fs.c#L1503
struct fs_ops {
    void (*free)(void *fc);
    int (*dup)(void *fc, void *src_fc);
    int (*parse_param)(void *fc, void *param);
    int (*parse_monolithic)(void *fc, void *data);
};

struct fs_ops_1 {
    void (*free)(void *fc);
    int (*dup)(void *fc, void *src_fc);
    int (*parse_param)(void *fc, void *param);
    int (*parse_monolithic)(void *fc, void *data);
};

struct fs_ctx {
    struct fs_ops *ops;
    int dummy;
};

void fs_test_free(void *fc);
int fs_test_dup(void *fc, void *src_fc);
int fs_test_parse_monolithic(void *fc, void *data);

static struct fs_ops_1 my_ops_obj = {
        .free = fs_test_free,
        .dup = fs_test_dup,
        .parse_monolithic = fs_test_parse_monolithic
};

void register_fs(struct fs_ctx *ctx) {
    ctx->ops = (struct fs_ops*)&my_ops_obj;
}

