#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 1000
#define ARRAY_LENGTH 300

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

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

static inline bn *copy_data_long(bn *result)
{
    bn *dup = vmalloc(sizeof(bn));
    dup->length = result->length;
    dup->num_arr = vmalloc(sizeof(ullong) * result->length);
    for (int i = 0; i < result->length; i++)
        *(dup->num_arr + i) = RESULT_INDEX(i);
    return dup;
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

////////////////////////// difference between v0 and v1
////////////////////////////////////////

static inline bn *create_bn_v1(ullong x)
{
    bn *new = kmalloc(sizeof(bn), GFP_KERNEL);
    new->length = 1;
    new->num_arr = kmalloc(sizeof(ullong), GFP_KERNEL);
    *(new->num_arr + 0) = x;
    return new;
}

static inline void free_bn_v1(bn *x)
{
    kfree(x->num_arr);
    kfree(x);
}

static inline void handle_carry_long_v1(bn *result)
{
    for (int i = 0; i < result->length - 1; i++) {
        RESULT_INDEX(i + 1) += RESULT_INDEX(i) / ULL_CARRY;
        RESULT_INDEX(i) %= ULL_CARRY;
    }
    if (RESULT_INDEX(result->length - 1) > ULL_CARRY) {
        ullong *new_array =
            kmalloc(sizeof(ullong) * (result->length + 1), GFP_KERNEL);
        for (int i = 0; i < result->length - 1; i++)
            new_array[i] = RESULT_INDEX(i);
        new_array[result->length] =
            RESULT_INDEX(result->length - 1) / ULL_CARRY;
        new_array[result->length - 1] =
            RESULT_INDEX(result->length - 1) % ULL_CARRY;
        result->length++;
        kfree(result->num_arr);
        result->num_arr = new_array;
    }
}

static inline bn *copy_data_long_v1(bn *result)
{
    bn *dup = kmalloc(sizeof(bn), GFP_KERNEL);
    dup->length = result->length;
    dup->num_arr = kmalloc(sizeof(ullong) * result->length, GFP_KERNEL);
    for (int i = 0; i < result->length; i++)
        *(dup->num_arr + i) = RESULT_INDEX(i);
    return dup;
}

static inline bn *add_bn_2_v1(bn *summand, bn *result)
{
    for (int i = 0; i < summand->length; i++)
        RESULT_INDEX(i) += *(summand->num_arr + i);
    handle_carry_long_v1(result);
    return result;
}

static inline bn *sub_bn_2_v1(bn *small, bn *result)  // result = result - small
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
        ullong *new_array = kmalloc(sizeof(ullong) * new_length, GFP_KERNEL);
        for (int i = 0; i < new_length; i++)
            *(new_array + i) = RESULT_INDEX(i);
        kfree(result->num_arr);
        result->num_arr = new_array;
        result->length = new_length;
    }
    return result;
}

static inline bn *multiply_by2_v1(bn *result)
{
    for (int i = 0; i < result->length; i++)
        RESULT_INDEX(i) <<= 1;
    handle_carry_long_v1(result);
    return result;
}

static inline bn *square_long_v1(bn *result)
{
    bn *dup = copy_data_long_v1(result);
    bn *tmp = copy_data_long_v1(result);
    kfree(result->num_arr);
    result->length <<= 1;
    result->num_arr = kmalloc(sizeof(ullong) * result->length--, GFP_KERNEL);
    for (int i = 0; i < result->length; i++)
        RESULT_INDEX(i) = 0ULL;
    for (int i = 0; i < tmp->length; i++)
        RESULT_INDEX(i) = *(tmp->num_arr + i) * *(dup->num_arr + 0);
    handle_carry_long_v1(result);
    for (int i = 1; i < dup->length; i++) {
        for (int j = 0; j < tmp->length; j++)
            *(tmp->num_arr + j) *= ULL_CARRY;
        handle_carry_long_v1(tmp);
        for (int j = 0; j < tmp->length; j++)
            RESULT_INDEX(j) += *(tmp->num_arr + j) * *(dup->num_arr + i);
        handle_carry_long_v1(result);
    }
    free_bn_v1(dup);
    free_bn_v1(tmp);
    return result;
}

static inline bn *multiplication_v1(bn *multiplicand, bn *result)
{
    bn *dup = copy_data_long_v1(result);
    bn *tmp = copy_data_long_v1(multiplicand);
    kfree(result->num_arr);
    result->length += multiplicand->length;
    result->num_arr = kmalloc(sizeof(ullong) * result->length--, GFP_KERNEL);
    for (int i = 0; i < result->length; i++)
        RESULT_INDEX(i) = 0ULL;
    for (int i = 0; i < tmp->length; i++)
        RESULT_INDEX(i) = *(tmp->num_arr + i) * *(dup->num_arr + 0);
    handle_carry_long_v1(result);
    for (int i = 1; i < dup->length; i++) {
        for (int j = 0; j < tmp->length; j++)
            *(tmp->num_arr + j) *= ULL_CARRY;
        handle_carry_long_v1(tmp);
        for (int j = 0; j < tmp->length; j++)
            RESULT_INDEX(j) += *(tmp->num_arr + j) * *(dup->num_arr + i);
        handle_carry_long_v1(result);
    }
    free_bn_v1(dup);
    free_bn_v1(tmp);
    return result;
}

static long long fib_fast_db_long(long long k, const char *buf)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    bn *f0 = create_bn(0ULL);
    bn *f1 = create_bn(1ULL);
    // char tmp[ARRAY_LENGTH];

    if (k == 0) {
        free_bn(f1);
        snprintf(buf, ARRAY_LENGTH, "%llu", *(f0->num_arr + f0->length - 1));
        // copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
        free_bn(f0);
        return 1;
    }
    if (k < 3) {
        free_bn(f0);
        snprintf(buf, ARRAY_LENGTH, "%llu", *(f1->num_arr + f1->length - 1));
        // copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
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
    snprintf(buf, ARRAY_LENGTH, "%llu", *(f0->num_arr + f0->length - 1));
    int l = strlen(buf);
    int w = 0;
    for (int i = f0->length - 2; i >= 0; i--) {
        snprintf(buf + l + w, ARRAY_LENGTH, "%09llu", *(f0->num_arr + i));
        w += 9;
    }
    // snprintf(tmp + l + w, ARRAY_LENGTH, "\n");
    // copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
    free_bn(f0);
    return 1;
}

static long long fib_fast_db_long_clz(long long k, const char *buf)
{
    bn *f0 = create_bn(0ULL);
    bn *f1 = create_bn(1ULL);

    if (k == 0) {
        free_bn(f1);
        snprintf(buf, ARRAY_LENGTH, "%llu", *(f0->num_arr + f0->length - 1));
        free_bn(f0);
        return 1;
    }
    if (k < 3) {
        free_bn(f0);
        snprintf(buf, ARRAY_LENGTH, "%llu", *(f1->num_arr + f1->length - 1));
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
    snprintf(buf, ARRAY_LENGTH, "%llu", *(f0->num_arr + f0->length - 1));
    int l = strlen(buf);
    int w = 0;
    for (int i = f0->length - 2; i >= 0; i--) {
        snprintf(buf + l + w, ARRAY_LENGTH, "%09llu", *(f0->num_arr + i));
        w += 9;
    }
    free_bn(f0);
    return 1;
}

static long long fib_fast_db_long_clz_v1(long long k, const char *buf)
{
    bn *f0 = create_bn_v1(0ULL);
    bn *f1 = create_bn_v1(1ULL);

    if (k == 0) {
        free_bn_v1(f1);
        snprintf(buf, ARRAY_LENGTH, "%llu", *(f0->num_arr + f0->length - 1));
        free_bn_v1(f0);
        return 1;
    }
    if (k < 3) {
        free_bn_v1(f0);
        snprintf(buf, ARRAY_LENGTH, "%llu", *(f1->num_arr + f1->length - 1));
        free_bn_v1(f1);
        return 1;
    }

    int x = 64 - __builtin_clzll(k);
    long long what = 1 << (x - 1);

    for (int i = 0; i < x; i++) {
        bn *temp0 = copy_data_long_v1(f0);
        bn *temp1 = copy_data_long_v1(f1);
        f0 = multiplication_v1(sub_bn_2_v1(f0, multiply_by2_v1(temp1)), f0);
        f1 = add_bn_2_v1(square_long_v1(temp0), square_long_v1(f1));
        if (k & what) {
            add_bn_2_v1(f0, f1);
            bn *temp2 = copy_data_long_v1(f1);
            sub_bn_2_v1(f0, temp2);
            free_bn_v1(f0);
            f0 = temp2;
        }
        what >>= 1;
        free_bn_v1(temp0);
        free_bn_v1(temp1);
    }
    free_bn_v1(f1);
    snprintf(buf, ARRAY_LENGTH, "%llu", *(f0->num_arr + f0->length - 1));
    int l = strlen(buf);
    int w = 0;
    for (int i = f0->length - 2; i >= 0; i--) {
        snprintf(buf + l + w, ARRAY_LENGTH, "%09llu", *(f0->num_arr + i));
        w += 9;
    }
    free_bn_v1(f0);
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
        fib_fast_db_long_clz(*offset, buf);
        ktime = ktime_sub(ktime_get(), ktime);
        break;
    case 1:
        ktime = ktime_get();
        fib_fast_db_long_clz_v1(*offset, buf);
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