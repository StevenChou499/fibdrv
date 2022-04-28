#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 500
#define ARRAY_LENGTH 120

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static long long fib_sequence(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long *f = vmalloc(sizeof(long long) * (k + 2));

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}

static long long fast_doubling(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long f[2] = {0, 1};

    if (k < 2)
        return k;

    int x = 64;
    long long one = 1LL << 62;

    while (!(k & one)) {
        one >>= 1;
        x--;
    }
    x--;

    for (int i = 0; i < x; i++) {
        long long temp;
        temp = f[0];
        f[0] = f[0] * (2 * f[1] - f[0]);
        f[1] = f[1] * f[1] + temp * temp;
        if (k & one) {
            f[1] = f[0] + f[1];
            f[0] = f[1] - f[0];
        }
        one >>= 1;
    }
    return f[0];
}

static long long fast_doubling_clz(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long f[2] = {0, 1};

    if (k < 2)
        return k;

    int x = 64 - __builtin_clzll(k);
    long long what = 1 << (x - 1);

    for (int i = 0; i < x; i++) {
        long long temp;
        temp = f[0];
        f[0] = f[0] * (2 * f[1] - f[0]);
        f[1] = f[1] * f[1] + temp * temp;
        if (k & what) {
            f[1] = f[0] + f[1];
            f[0] = f[1] - f[0];
        }
        what >>= 1;
    }
    return f[0];
}

#define MAX 120

typedef struct _BigN {
    int digits;
    char num[MAX];
} BigN;

static inline void reverse(BigN *str)
{
    char d = str->digits - 1;
    for (int i = 0; i < str->digits / 2; i++, d--) {
        *(str->num + i) ^= *(str->num + d);
        *(str->num + d) ^= *(str->num + i);
        *(str->num + i) ^= *(str->num + d);
    }
}

static inline void char_to_int(BigN *str)
{
    for (int i = 0; i < str->digits; i++) {
        *(str->num + i) -= 48;
    }
}

static inline void int_to_char(BigN *str)
{
    for (int i = 0; i < str->digits; i++) {
        *(str->num + i) += '0';
    }
}

static inline void handle_carry(
    BigN *str)  // the string is reversed and format is int
{
    for (int i = 0; i < str->digits; i++) {
        *(str->num + (i + 1)) += *(str->num + i) / 10;
        *(str->num + i) %= 10;
    }
    if (*(str->num + str->digits))
        str->digits++;
    *(str->num + str->digits) = '\0';
}

void addBigN(BigN *x, BigN *y, BigN *output)
{
    reverse(x);
    reverse(y);
    char_to_int(x);
    char_to_int(y);
    output->digits = x->digits > y->digits ? x->digits : y->digits;
    for (int i = 0; i < output->digits; i++) {
        *(output->num + i) = *(x->num + i) + *(y->num + i);
    }
    handle_carry(output);
    reverse(x);
    reverse(y);
    reverse(output);
    int_to_char(x);
    int_to_char(y);
    int_to_char(output);
}

static long long fib_sequence_str(long long k, const char *buf)
{
    BigN *f = (BigN *) vmalloc(sizeof(BigN) * (k + 2));
    for (int i = 0; i < k + 2; i++) {
        memset(f[i].num, 0, MAX);
    }

    f[0].digits = 1;
    *(f[0].num + 0) = '0';
    *(f[0].num + 1) = 0;
    f[1].digits = 1;
    *(f[1].num + 0) = '1';
    *(f[1].num + 1) = 0;

    if (k < 2) {
        copy_to_user(buf, f[k].num, 120);
        return (long long) f[k].num;
    }

    for (int i = 2; i <= k; i++) {
        addBigN(&f[i - 2], &f[i - 1], &f[i]);
    }
    copy_to_user((void *) buf, f[k].num, 120);
    vfree(f);
    return 1;
    // return f[k].digits;
}

static inline void copy_data(BigN *copier, BigN *copied)
{
    for (int i = 0; i <= copied->digits; i++)
        *(copier->num + i) = *(copied->num + i);
    copier->digits = copied->digits;
}

void addBigN_3(BigN *x, BigN *y, BigN *output)
{
    reverse(x);
    reverse(y);
    char_to_int(x);
    char_to_int(y);
    output->digits = x->digits > y->digits ? x->digits : y->digits;
    for (int i = 0; i < output->digits; i++) {
        *(output->num + i) = *(x->num + i) + *(y->num + i);
    }
    handle_carry(output);
    reverse(x);
    reverse(y);
    reverse(output);
    int_to_char(x);
    int_to_char(y);
    int_to_char(output);
}

void addBigN_2(BigN *x, BigN *output)
{
    reverse(x);
    char_to_int(x);
    reverse(output);
    char_to_int(output);
    output->digits = x->digits > output->digits ? x->digits : output->digits;
    for (int i = 0; i < output->digits; i++) {
        *(output->num + i) = *(x->num + i) + *(output->num + i);
    }
    handle_carry(output);
    reverse(x);
    reverse(output);
    int_to_char(x);
    int_to_char(output);
}

static inline void handle_regroup(
    BigN *str)  // the string is reversed and format is int
{
    for (int i = 0; i < str->digits; i++) {
        if (*(str->num + i) < 0) {
            *(str->num + i) += 10;
            *(str->num + (i + 1)) -= 1;
        }
    }
    while (!*(str->num + str->digits))
        str->digits--;
    *(str->num + ++str->digits) = '\0';
}

static inline BigN *multiply_by_2(BigN *output)
{
    reverse(output);
    for (int i = 0; i < output->digits; i++) {
        *(output->num + i) -= '0';
        *(output->num + i) <<= 1;
    }
    handle_carry(output);
    for (int i = 0; i < output->digits; i++) {
        *(output->num + i) += '0';
    }
    reverse(output);
    return output;
}

static inline BigN *square(BigN *side)
{
    reverse(side);
    char_to_int(side);
    BigN *temp0 = vmalloc(sizeof(BigN));
    BigN *temp1 = vmalloc(sizeof(BigN));
    copy_data(temp0, side);
    copy_data(temp1, side);
    memset(side->num, 0, MAX);
    int length = temp0->digits;
    for (int j = 0; j < temp0->digits; j++) {
        *(side->num + j) += *(temp0->num + j) * *(temp1->num + 0);
        handle_carry(side);
    }
    for (int i = 1; i < length; i++) {
        for (int j = 0; j < temp0->digits; j++) {
            *(temp0->num + j) *= 10;
        }
        handle_carry(temp0);
        side->digits =
            side->digits > temp0->digits ? side->digits : temp0->digits;
        for (int j = 0; j < temp0->digits; j++) {
            *(side->num + j) += *(temp0->num + j) * *(temp1->num + i);
            handle_carry(side);
        }
    }
    vfree(temp0);
    vfree(temp1);
    reverse(side);
    int_to_char(side);
    return side;
}

static inline BigN *multiplication_2(BigN *x, BigN *output)
{
    reverse(x);
    char_to_int(x);
    reverse(output);
    char_to_int(output);
    BigN *temp0 = vmalloc(sizeof(BigN));
    BigN *temp1 = vmalloc(sizeof(BigN));
    copy_data(temp0, x);
    copy_data(temp1, output);
    memset(output->num, 0, MAX);
    output->digits = output->digits > x->digits ? output->digits : x->digits;
    for (int j = 0; j < temp0->digits; j++) {
        *(output->num + j) += *(temp0->num + j) * *(temp1->num + 0);
    }
    handle_carry(output);
    for (int i = 1; i < temp1->digits; i++) {
        for (int j = 0; j < temp0->digits; j++) {
            *(temp0->num + j) *= 10;
        }
        handle_carry(temp0);
        output->digits =
            output->digits > temp0->digits ? output->digits : temp0->digits;
        for (int j = 0; j < temp0->digits; j++) {
            *(output->num + j) += *(temp0->num + j) * *(temp1->num + i);
            handle_carry(output);
        }
        handle_carry(output);
    }
    vfree(temp0);
    vfree(temp1);
    reverse(output);
    int_to_char(output);
    reverse(x);
    int_to_char(x);
    return output;
}

void minusBigN_2_former(BigN *output, BigN *x)  // output > x
{
    reverse(x);
    reverse(output);
    char_to_int(x);
    char_to_int(output);
    output->digits = x->digits;
    for (int i = 0; i < x->digits; i++) {
        *(output->num + i) = *(output->num + i) - *(x->num + i);
    }
    handle_regroup(output);
    reverse(x);
    reverse(output);
    int_to_char(x);
    int_to_char(output);
}

void minusBigN_2_latter(BigN *x, BigN *output)  // x > output
{
    reverse(x);
    reverse(output);
    char_to_int(x);
    char_to_int(output);
    output->digits = x->digits;
    for (int i = 0; i < x->digits; i++) {
        *(output->num + i) = *(x->num + i) - *(output->num + i);
    }
    handle_regroup(output);
    reverse(x);
    reverse(output);
    int_to_char(x);
    int_to_char(output);
}

#define ULL_CARRY 1000000000ULL
#define ULL_MAX 18446744073709551615ULL
#define RESULT_INDEX(i) *(result->num_arr + i)

typedef unsigned long long ullong;

typedef struct _bn {
    char length;
    unsigned long long *num_arr;
} bn;

static inline bn *create_bn(ullong x)
{
    bn *new = vmalloc(sizeof(bn));
    new->length = 1;
    new->num_arr = vmalloc(sizeof(ullong));
    *(new->num_arr + 0) = x;
    return new;
}

static inline void free_bn(bn *x)
{
    vfree(x->num_arr);
    vfree(x);
}

static inline void handle_carry_long(bn *result)
{
    for (int i = 0; i < result->length - 1; i++) {
        RESULT_INDEX(i + 1) += RESULT_INDEX(i) / ULL_CARRY;
        RESULT_INDEX(i) %= ULL_CARRY;
    }
    if (RESULT_INDEX(result->length - 1) > ULL_CARRY) {
        ullong *new_array = vmalloc(sizeof(ullong) * (result->length + 1));
        for (int i = 0; i < result->length - 1; i++)
            new_array[i] = RESULT_INDEX(i);
        new_array[result->length] =
            RESULT_INDEX(result->length - 1) / ULL_CARRY;
        new_array[result->length - 1] =
            RESULT_INDEX(result->length - 1) % ULL_CARRY;
        result->length++;
        vfree(result->num_arr);
        result->num_arr = new_array;
    }
}

/*
static inline bn *add_bn_3(bn *small,
                           bn *large,
                           bn *result)  // result = large + small
{
    // result = vmalloc(sizeof(bn));  // allocate space for result
    result->length = large->length;
    result->num_arr = vmalloc(sizeof(ullong) * result->length);

    for (int i = 0; i < small->length; i++)
        RESULT_INDEX(i) = *(large->num_arr + i) + *(small->num_arr + i);
    for (int i = small->length; i < large->length; i++)
        RESULT_INDEX(i) = *(large->num_arr + i);
    handle_carry_long(result);
    return result;
}*/

static inline bn *copy_data_long(bn *result)
{
    bn *dup = vmalloc(sizeof(bn));
    dup->length = result->length;
    dup->num_arr = vmalloc(sizeof(ullong) * result->length);
    for (int i = 0; i < result->length; i++)
        *(dup->num_arr + i) = RESULT_INDEX(i);
    return dup;
}

static inline void copy_data_long_2(bn *result, bn *source)
{
    source->length = result->length;
    source->num_arr = vmalloc(sizeof(ullong) * result->length);
    for (int i = 0; i < result->length; i++)
        *(source->num_arr + i) = RESULT_INDEX(i);
}

static inline bn *add_bn_2(bn *summand, bn *result)
{
    for (int i = 0; i < summand->length; i++)
        RESULT_INDEX(i) += *(summand->num_arr + i);
    handle_carry_long(result);
    return result;
}

static inline bn *sub_bn_2(bn *small, bn *result)  // result = result - small
{
    for (int i = 0; i < small->length; i++)
        RESULT_INDEX(i) = RESULT_INDEX(i) - *(small->num_arr + i);
    for (int i = 0; i < small->length; i++) {
        if (RESULT_INDEX(i) > ULL_CARRY) {
            RESULT_INDEX(i + 1) -= 1;
            RESULT_INDEX(i) += ULL_CARRY;
        }
    }
    int new_length = result->length - 1;
    if (!RESULT_INDEX(new_length)) {
        while (!RESULT_INDEX(new_length))
            new_length--;
        new_length++;
        ullong *new_array = vmalloc(sizeof(ullong) * new_length);
        for (int i = 0; i < new_length; i++)
            *(new_array + i) = RESULT_INDEX(i);
        vfree(result->num_arr);
        result->num_arr = new_array;
        result->length = new_length;
    }
    return result;
}

static inline bn *multiply_by2(bn *result)
{
    for (int i = 0; i < result->length; i++)
        RESULT_INDEX(i) <<= 1;
    handle_carry_long(result);
    return result;
}

static inline bn *square_long(bn *result)
{
    bn *dup = copy_data_long(result);
    bn *tmp = copy_data_long(result);
    vfree(result->num_arr);
    result->length <<= 1;
    result->num_arr = vmalloc(sizeof(ullong) * result->length--);
    for (int i = 0; i < result->length; i++)
        RESULT_INDEX(i) = 0ULL;
    for (int i = 0; i < tmp->length; i++)
        RESULT_INDEX(i) = *(tmp->num_arr + i) * *(dup->num_arr + 0);
    handle_carry_long(result);
    for (int i = 1; i < dup->length; i++) {
        for (int j = 0; j < tmp->length; j++)
            *(tmp->num_arr + j) *= ULL_CARRY;
        handle_carry_long(tmp);
        for (int j = 0; j < tmp->length; j++)
            RESULT_INDEX(j) += *(tmp->num_arr + j) * *(dup->num_arr + i);
        handle_carry_long(result);
    }
    free_bn(dup);
    free_bn(tmp);
    return result;
}

static inline bn *multiplication(bn *multiplicand, bn *result)
{
    bn *dup = copy_data_long(result);
    bn *tmp = copy_data_long(multiplicand);
    vfree(result->num_arr);
    result->length += multiplicand->length;
    result->num_arr = vmalloc(sizeof(ullong) * result->length--);
    for (int i = 0; i < result->length; i++)
        RESULT_INDEX(i) = 0ULL;
    for (int i = 0; i < tmp->length; i++)
        RESULT_INDEX(i) = *(tmp->num_arr + i) * *(dup->num_arr + 0);
    handle_carry_long(result);
    for (int i = 1; i < dup->length; i++) {
        for (int j = 0; j < tmp->length; j++)
            *(tmp->num_arr + j) *= ULL_CARRY;
        handle_carry_long(tmp);
        for (int j = 0; j < tmp->length; j++)
            RESULT_INDEX(j) += *(tmp->num_arr + j) * *(dup->num_arr + i);
        handle_carry_long(result);
    }
    free_bn(dup);
    free_bn(tmp);
    return result;
}

/*void print_number(bn *x)
{
    printf("%llu", *(x->num_arr + x->length - 1));
    for(char i = x->length - 2; i >= 0; i--){
        printf("%018llu", *(x->num_arr + i));
    }
    printf("\n");
    free_bn(x);
}*/

static long long fib_long_iter(long long k, const char *buf)
{
    bn *a, *b;
    char tmp[ARRAY_LENGTH];
    a = create_bn(0ULL);
    b = create_bn(1ULL);
    if (k == 0) {
        free_bn(b);
        snprintf(tmp, ARRAY_LENGTH, "%llu", *(a->num_arr + a->length - 1));
        free_bn(a);
        copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
        return 1;
    }
    if (k == 1 || k == 2) {
        free_bn(a);
        snprintf(tmp, ARRAY_LENGTH, "%llu", *(b->num_arr + b->length - 1));
        free_bn(b);
        copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
        return 1;
    }
    for (long long j = 0; j < k - 1ULL; j++) {
        bn temp;
        add_bn_2(a, b);
        copy_data_long_2(b, &temp);
        sub_bn_2(a, &temp);
        free_bn(a);
        a = vmalloc(sizeof(bn));
        copy_data_long_2(&temp, a);
    }
    snprintf(tmp, ARRAY_LENGTH, "%llu", *(b->num_arr + b->length - 1));
    int l = strlen(tmp);
    int w = 0;
    for (int i = b->length - 2; i >= 0; i--) {
        snprintf(tmp + l + w, ARRAY_LENGTH, "%018llu", *(b->num_arr + i));
        w += 18;
    }
    copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
    free_bn(a);
    free_bn(b);
    return 1;
}

static long long fib_fast_db_long(long long k, const char *buf)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    bn *f0 = create_bn(0ULL);
    bn *f1 = create_bn(1ULL);
    char tmp[ARRAY_LENGTH];

    if (k == 0) {
        free_bn(f1);
        snprintf(tmp, ARRAY_LENGTH, "%llu", *(f0->num_arr + f0->length - 1));
        copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
        free_bn(f0);
        return 1;
    }
    if (k < 3) {
        free_bn(f0);
        snprintf(tmp, ARRAY_LENGTH, "%llu", *(f1->num_arr + f1->length - 1));
        copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
        free_bn(f1);
        return 1;
    }

    int x = 64;
    long long what = 1LL << 62;
    while (!(k & what)) {
        what >>= 1;
        x--;
    }
    x--;

    for (int i = 0; i < x; i++) {
        bn *temp0 = copy_data_long(f0);
        bn *temp1 = copy_data_long(f1);
        f0 = multiplication(sub_bn_2(f0, multiply_by2(temp1)), f0);
        f1 = add_bn_2(square_long(temp0), square_long(f1));
        if (k & what) {
            add_bn_2(f0, f1);
            bn *temp2 = copy_data_long(f1);
            sub_bn_2(f0, temp2);
            free_bn(f0);
            f0 = temp2;
        }
        what >>= 1;
        free_bn(temp0);
        free_bn(temp1);
    }
    free_bn(f1);
    snprintf(tmp, ARRAY_LENGTH, "%llu", *(f0->num_arr + f0->length - 1));
    int l = strlen(tmp);
    int w = 0;
    for (int i = f0->length - 2; i >= 0; i--) {
        snprintf(tmp + l + w, ARRAY_LENGTH, "%09llu", *(f0->num_arr + i));
        w += 9;
    }
    // snprintf(tmp + l + w, ARRAY_LENGTH, "\n");
    copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
    free_bn(f0);
    return 1;
}

static long long fib_fast_db_long_clz(long long k, const char *buf)
{
    bn *f0 = create_bn(0ULL);
    bn *f1 = create_bn(1ULL);
    char tmp[ARRAY_LENGTH];

    if (k == 0) {
        free_bn(f1);
        snprintf(tmp, ARRAY_LENGTH, "%llu", *(f0->num_arr + f0->length - 1));
        copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
        free_bn(f0);
        return 1;
    }
    if (k < 3) {
        free_bn(f0);
        snprintf(tmp, ARRAY_LENGTH, "%llu", *(f1->num_arr + f1->length - 1));
        copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
        free_bn(f1);
        return 1;
    }

    int x = 64 - __builtin_clzll(k);
    long long what = 1 << (x - 1);

    for (int i = 0; i < x; i++) {
        bn *temp0 = copy_data_long(f0);
        bn *temp1 = copy_data_long(f1);
        f0 = multiplication(sub_bn_2(f0, multiply_by2(temp1)), f0);
        f1 = add_bn_2(square_long(temp0), square_long(f1));
        if (k & what) {
            add_bn_2(f0, f1);
            bn *temp2 = copy_data_long(f1);
            sub_bn_2(f0, temp2);
            free_bn(f0);
            f0 = temp2;
        }
        what >>= 1;
        free_bn(temp0);
        free_bn(temp1);
    }
    free_bn(f1);
    snprintf(tmp, ARRAY_LENGTH, "%llu", *(f0->num_arr + f0->length - 1));
    int l = strlen(tmp);
    int w = 0;
    for (int i = f0->length - 2; i >= 0; i--) {
        snprintf(tmp + l + w, ARRAY_LENGTH, "%09llu", *(f0->num_arr + i));
        w += 9;
    }
    copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
    free_bn(f0);
    return 1;
}

static long long fib_sequence_fast_db_str(long long k, const char *buf)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    BigN *f = vmalloc(sizeof(BigN) * 2);
    for (int i = 0; i < 2; i++) {
        memset(f[i].num, 0, MAX);
    }
    f[0].digits = 1;
    *(f[0].num + 0) = '0';
    *(f[0].num + 1) = 0;
    f[1].digits = 1;
    *(f[1].num + 0) = '1';
    *(f[1].num + 1) = 0;

    if (k < 2) {
        copy_to_user((void *) buf, f[k].num, 120);
        return 1;
    }

    int x = 64;
    long long what = 1LL << 62;
    while (!(k & what)) {
        what >>= 1;
        x--;
    }
    x--;

    static BigN temp0, temp1;

    for (int i = 0; i < x; i++) {
        memset(&temp0, 0, MAX);
        memset(&temp0, 0, MAX);
        copy_data(&temp0, &f[0]);
        copy_data(&temp1, &f[1]);
        minusBigN_2_former(multiply_by_2((f + 1)), (f + 0));
        multiplication_2((f + 1), (f + 0));
        addBigN_3(square(&temp1), square(&temp0), (f + 1));
        if (k & what) {
            addBigN_2((f + 0), (f + 1));
            minusBigN_2_latter((f + 1), (f + 0));
        }
        what >>= 1;
    }
    copy_to_user((void *) buf, f[0].num, 120);
    vfree(f);
    return 1;
}

static long long fib_sequence_fast_db_str_clz(long long k, const char *buf)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    BigN *f = vmalloc(sizeof(BigN) * 2);
    for (int i = 0; i < 2; i++) {
        memset(f[i].num, 0, MAX);
    }
    f[0].digits = 1;
    *(f[0].num + 0) = '0';
    *(f[0].num + 1) = 0;
    f[1].digits = 1;
    *(f[1].num + 0) = '1';
    *(f[1].num + 1) = 0;

    if (k < 2) {
        copy_to_user((void *) buf, f[k].num, 120);
        return 1;
    }

    int x = 64 - __builtin_clzll(k);
    long long what = 1 << (x - 1);

    static BigN temp0, temp1;

    for (int i = 0; i < x; i++) {
        memset(&temp0, 0, MAX);
        memset(&temp0, 0, MAX);
        copy_data(&temp0, &f[0]);
        copy_data(&temp1, &f[1]);
        minusBigN_2_former(multiply_by_2((f + 1)), (f + 0));
        multiplication_2((f + 1), (f + 0));
        addBigN_3(square(&temp1), square(&temp0), (f + 1));
        if (k & what) {
            addBigN_2((f + 0), (f + 1));
            minusBigN_2_latter((f + 1), (f + 0));
        }
        what >>= 1;
    }
    copy_to_user((void *) buf, f[0].num, 120);
    vfree(f);
    return 1;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    return (ssize_t) fib_fast_db_long(*offset, buf);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    ktime_t ktime;
    switch (size) {
    case 0:
        ktime = ktime_get();
        fib_sequence(*offset);
        ktime = ktime_sub(ktime_get(), ktime);
        break;
    case 1:
        ktime = ktime_get();
        fast_doubling(*offset);
        ktime = ktime_sub(ktime_get(), ktime);
        break;
    case 2:
        ktime = ktime_get();
        fast_doubling_clz(*offset);
        ktime = ktime_sub(ktime_get(), ktime);
        break;
    case 3:
        ktime = ktime_get();
        fib_sequence_str(*offset, buf);
        ktime = ktime_sub(ktime_get(), ktime);
        break;
    case 4:
        ktime = ktime_get();
        fib_sequence_fast_db_str(*offset, buf);
        ktime = ktime_sub(ktime_get(), ktime);
        break;
    case 5:
        ktime = ktime_get();
        fib_sequence_fast_db_str_clz(*offset, buf);
        ktime = ktime_sub(ktime_get(), ktime);
        break;
    case 6:
        ktime = ktime_get();
        fib_long_iter(*offset, buf);
        ktime = ktime_sub(ktime_get(), ktime);
        break;
    case 7:
        ktime = ktime_get();
        fib_fast_db_long(*offset, buf);
        ktime = ktime_sub(ktime_get(), ktime);
        break;
    case 8:
        ktime = ktime_get();
        fib_fast_db_long_clz(*offset, buf);
        ktime = ktime_sub(ktime_get(), ktime);
        break;
    default:
        return 1;
    }
    return (ssize_t) ktime_to_ns(ktime);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);