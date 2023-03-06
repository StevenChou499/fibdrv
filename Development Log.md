# fibdrv
contributed by < `StevenChou499` >

## 實驗環境
```shell
$ gcc --version
gcc (Ubuntu 9.4.0-1ubuntu1~20.04) 9.4.0

$ lscpu
Architecture:                    x86_64
CPU op-mode(s):                  32-bit, 64-bit
Byte Order:                      Little Endian
Address sizes:                   39 bits physical, 48 bits virtual
CPU(s):                          4
On-line CPU(s) list:             0-3
Thread(s) per core:              1
Core(s) per socket:              4
Socket(s):                       1
NUMA node(s):                    1
Vendor ID:                       GenuineIntel
CPU family:                      6
Model:                           60
Model name:                      Intel(R) Core(TM) i5-4690 CPU @ 3.50GHz
Stepping:                        3
CPU MHz:                         3500.000
CPU max MHz:                     3900.0000
CPU min MHz:                     800.0000
BogoMIPS:                        6984.02
L1d cache:                       128 KiB
L1i cache:                       128 KiB
L2 cache:                        1 MiB
L3 cache:                        6 MiB
NUMA node0 CPU(s):               0-3
```

## 自我檢查清單

- [x] 研讀上述 ==Linux 效能分析的提示== 描述，在自己的實體電腦運作 GNU/Linux，做好必要的設定和準備工作→從中也該理解為何不希望在虛擬機器中進行實驗;

### 查看行程的 CPU affinity 

查看特定行程的處理器親和性，輸入以下指令(以 PID 為 `1` 為例)：
```shell
$ taskset -p 1
```

輸出為：
```
pid 1's current affinity mask: f
```

由 `f` 等於 `1111` 可以知道與實際核心數為 `4` 個相吻合。

### 將行程固定在特定的 CPU 中執行

設定 PID = 1 的行程固定執行在 #CPU0，輸入以下指令：
```shell
$ taskset -p 0x1 2458
```

得到以下輸出：
```
pid 2458's current affinity mask: f
pid 2458's new affinity mask: 1
```

成功將 pid 2458 的 cpu affinity 改成 #CPU0

### 排除干擾效能分析的因素
 * 抑制 [address space layout randomization](https://en.wikipedia.org/wiki/Address_space_layout_randomization) (ASLR)
```shell
$ sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"
```

 * 設定 scaling_governor 為 performance。準備以下 shell script，檔名為 `performance.sh` ：
```shell
for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
do
    echo performance > ${i}
done
```
之後用 `$ sudo sh performance.sh` 執行

 * 針對 Intel 處理器，關閉 turbo mode
```shell
$ sudo sh -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"
```

為何虛擬機器不適合進行這次的實驗，因為虛擬機器所使用的[排程器](https://en.wikipedia.org/wiki/Scheduling_(computing)) (scheduler) 其實是針對虛擬的 CPU ，而虛擬的 CPU 還需要再經過一次排程才會進入真正物理上的實體 CPU ，因此又稱為 "double scheduling" 。這樣不僅對效能會打折扣，對於實際上的效能分析結果也已經失真。
> And what are virtual CPUs? Well, in most platforms they are also a kind of special task and they want to run on some CPUs ... therefore we need a scheduler for that! This is usually called the "double-scheduling" property of systems employing virtualization because, well, there literally are two schedulers: one — let us call it the host scheduler, or the hypervisor scheduler — that schedules the virtual CPUs on the host physical CPUs; and another one — let us call it the guest scheduler — that schedules the guest OS's tasks on the guest's virtual CPUs.

reference: [Virtual-machine scheduling and scheduling in virtual machines](https://lwn.net/Articles/793375/)

- [x] 研讀上述費氏數列相關材料 (包含論文)，摘錄關鍵手法，並思考 [clz / ctz](https://en.wikipedia.org/wiki/Find_first_set) 一類的指令對 Fibonacci 數運算的幫助。請列出關鍵程式碼並解說

經過研讀論文以及查看關於費氏數列的文章後，我們可以知道計算特定位置的費氏數列最快的速度為 [Fast doubling](https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling) 的方法，而詳細方法是觀看 Youtube 上的一部[影片](https://www.youtube.com/watch?v=Mrrgpz2rBjQ)，影片中有提到 [Fast doubling](https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling) 的真正算法以及為何會與 [clz / ctz](https://en.wikipedia.org/wiki/Find_first_set) 有關。其實做方式如下(以數字 `13` 為例)：

 `13` 之二進位為 `1101` 。
```
                    需要計算的數字   13之二進位
                    f(13)        |
                                 |
                    f(6)  f(7)   |  1
                                 |
                    f(3)  f(4)   |  0
                                 |
                    f(1)  f(2)   |  1
                                 |
                    f(0)  f(1)   |  1
```
由：
\begin{aligned}	
F(2k)=F(k)[2F(k+1)-F(k)] \\
F(2k+1)=F(k+1)^2+F(k)^2
\end{aligned}	
若需要計算出 `f(13)` 之值為何，此時之 `k` 值為 `6` ，因此我們需要 `f(6)` 以及 `f(7)` 之值，而在計算 `f(6)` 之值時，此時之 `k` 值為 `3` ，因此需要 `f(3)` 以及 `f(4)` 之值，計算 `f(3)` 之值時需要 `f(1)` 與 `f(2)` 之值，計算 `f(1)` 之值時需要 `f(0)` 與 `f(1)` 之值。了解其關係後我們可以開始回推，因為費氏數列之 `f(0)` 以及 `f(1)` 已經為確定之數值，此時之 `k = 0` ，因此我們可以透過 [Fast doubling](https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling) 的方式計算出 `f(2*0)` 與 `f(2*0+1)` ，也就是 `f(0)` 與 `f(1)` ，而沒有辦法計算 `f(2)` 的數值，此時我們必須透過最原始的關係，即

\begin{aligned}	
F(k+2)=F(k+1)+F(k)
\end{aligned}

來計算出 `f(2)` 的數值，在擁有 `f(1)` 與 `f(2)` 之數值後，此時之 `k = 1` ，我們便可以計算 `f(3)` 之值，同樣的必須透過原始定義計算 `f(4)` 。接著 `k = 3` ，我們可以直接計算出 `f(6)` 與 `f(7)` 之值，最後 `k = 6` ，可以計算出 `f(13)` 之值。

從以上的規律我們可以看到， [Fast doubling](https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling) 剛好為每次切半再做計算，而計算之次數會與該數之二進位位元數相同。而在做計算時，若遇到位元值為 `0` 時，我們可以直接使用 [Fast doubling](https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling) 之方法即可，而若位元值為 `1` ，則必須在使用 [Fast doubling](https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling) 計算完後，再將計算結果相加才算計算完成。

而剛剛說道 [Fast doubling](https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling) 的次數會與其二進位制中有效位元的數量，此時便可以利用 [clz / ctz](https://en.wikipedia.org/wiki/Find_first_set) 計算出共有幾個 leading zeros ，以方便加快計算速度，只須計算有效之位元即可。

以下為使用 [Fast doubling](https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling) 的程式實做方法(也只能到 `f(92)` )：
```c
static long long fib_sequence(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long f[2] = {0, 1};
    long long temp;

    if(k < 2)
        return k;
    
    int x = 64 - __builtin_clzll(k);
    long long what = 1 << x-1;
    
    for(int i = 0; i < x; i++){
        temp = f[0];
        f[0] = f[0]*(2*f[1]-f[0]);
        f[1] = f[1]*f[1] + temp*temp;
        if(k & what){
            f[1] = f[0] + f[1];
            f[0] = f[1] - f[0];
        }
        what >>= 1;
    }
    return f[0];
}
```

以下為輸出結果
```shell
$ gcc -o testing testing.c
$ /.testing
fib(0) = 0
fib(1) = 1
fib(2) = 1
fib(3) = 2
.
.
.
fib(90) = 2880067194370816120
fib(91) = 4660046610375530309
fib(92) = 7540113804746346429
```

程式碼中便有使用到 `__builtin_clzll(k)` 的巨集，以方便計算共有幾個有效位元。

- [ ] 複習 [C 語言數值系統](https://hackmd.io/@sysprog/c-numerics) 和 [bitwise operation](https://hackmd.io/@sysprog/c-bitwise)，思考 Fibonacci 數快速計算演算法的實作中如何減少乘法運算的成本

研讀 [C 語言數值系統](https://hackmd.io/@sysprog/c-numerics) 與 [bitwise operation](https://hackmd.io/@sysprog/c-bitwise) 後，

## 撰寫 Linux 核心模組

經過驗算 [Fast Doubling](https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling) 的可行性後，我們可以進行比較兩者的速度。

以下為兩者在 0~92 之計算速度，我們可以看到一開始兩者的速度是差不多的，但是當數字越來越大時，可以明顯看出兩者之差距， [Fast Doubling](https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling) 的速度會快非常多
![](https://i.imgur.com/fF2A7bH.png)

### 固定程式執行於特定CPU
#### 先將程式固定於一 CPU 做執行

先到 `/etc/default/`  ，並在終端機輸入：
```shell
$ sudo -H gedit grub
```
便可以在只能讀的檔案中進行寫的操作，找到
```
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"
```
修改成：
```
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash isolcpus=3" //讓第3顆CPU在重開機後不運作
```

#### 更新 `/boot/grub/grub.cfg` 
常規方法為：
```shell
$ sudo upadte-grub
```
若沒有更新成功，可以使用另一種更新方法：
```shell
$ sudo grub-mkconfig -o /boot/grub/grub.cfg
```

#### 重啟系統
終端機輸入：
```shell
$ reboot
```

#### 檢查是否生效
終端機輸入：
```shell
$ taskset -cp 1
```
若其顯示：
```shell
pid 1's current affinity list: 0-2 #第3個CPU被隔離無法使用
```
代表第3個CPU已經被隔離。

### 排除干擾效能分析的因素
 * 抑制 [address space layout randomization](https://en.wikipedia.org/wiki/Address_space_layout_randomization) (ASLR)
```shell
$ sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"
```
 * 設定 scaling_governor 為 performance。準備以下 shell script，檔名為 `performance.sh` ：
```shell
for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
do
    echo performance > ${i}
done
```
之後再用 `$ sudo sh performance.sh` 執行
 * 針對 Intel 處理器，關閉 Turbo Mode
```shell
$ sudo sh -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"
```

### Fast Doubling 運行速度
透過參考 [KYG-yaya573142 的報告](https://hackmd.io/@KYWeng/rkGdultSU) ，我先創立一個名為 `client_plot.c` 的檔案，以方便進行測試一般 Fibonacci 、 Fast Doubling 與 使用 [clz / ctz](https://en.wikipedia.org/wiki/Find_first_set) 的 Fast Doubling。
以下為 `client_plot.c` 的內容：
```c
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char buf[1];
    int offset = 100; /* TODO: try test something bigger than the limit */
    struct timespec start, end;

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        long long sz;
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &start); // Normal Fibonacci
        sz = write(fd, buf, 0);
        clock_gettime(CLOCK_MONOTONIC, &end);
        printf("%lld ", (long long) ((end.tv_sec * 1e9 + end.tv_nsec) -
                                     (start.tv_sec * 1e9 + start.tv_nsec)));
        printf("%lld ", sz);
        printf("%lld ", (long long) ((end.tv_sec * 1e9 + end.tv_nsec) -
                                     (start.tv_sec * 1e9 + start.tv_nsec)) -
                            sz);

        clock_gettime(CLOCK_MONOTONIC, &start); // Fast Doubling
        sz = write(fd, buf, 1);
        clock_gettime(CLOCK_MONOTONIC, &end);
        printf("%lld ", (long long) ((end.tv_sec * 1e9 + end.tv_nsec) -
                                     (start.tv_sec * 1e9 + start.tv_nsec)));
        printf("%lld ", sz);
        printf("%lld ", (long long) ((end.tv_sec * 1e9 + end.tv_nsec) -
                                     (start.tv_sec * 1e9 + start.tv_nsec)) -
                            sz);

        clock_gettime(CLOCK_MONOTONIC, &start); // Fast Doubling with clz
        sz = write(fd, buf, 2);
        clock_gettime(CLOCK_MONOTONIC, &end);
        printf("%lld ", (long long) ((end.tv_sec * 1e9 + end.tv_nsec) -
                                     (start.tv_sec * 1e9 + start.tv_nsec)));
        printf("%lld ", sz);
        printf("%lld\n", (long long) ((end.tv_sec * 1e9 + end.tv_nsec) -
                                      (start.tv_sec * 1e9 + start.tv_nsec)) -
                             sz);
    }

    close(fd);
    return 0;
}
```
撰寫完 `client_plot.c` 檔後，再更改 `fibdrv.c` 以改變費波那契數的算法，除了原本的 `fib_sequence()` 外，再加上 `fast_doubling()` 與 `fast_doubling_clz()` ，並透過 `fib_write()` 去實做，以下分別為其實做方法：
 *  `fast_doubling()` :
```c
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
```

 *  `fast_doubling_clz()` :
```c
static long long fast_doubling_clz(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long f[2] = {0, 1};

    if (k < 2)
        return k;

    int x = 64 - __builtin_clzll(k);
    long long what = 1 << x - 1;

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
```
 *  `fib_write()` :
```c
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
    }
    return (ssize_t) ktime;
}
```
藉由修改 `client_plot.c` 與 `fibdrv.c` ，我們可以分別測量三種情況下程式在 `userspace` 、 `kernel space` 與 `kernel to user space` 的時間，為了避免一直輸入相同的指令與加快執行相同指令的效率，我修改一下 `Makefile` ，加入了 `make test` 的指令：
```makefile
test:
	$(MAKE) unload
	make
	gcc -o client_plot client_plot.c
	$(MAKE) load
	sudo taskset 0x3 ./client_plot > out
	$(MAKE) unload
	gnuplot fib_nor.gp
	gnuplot fib_fast.gp
	gnuplot fib_fast_clz.gp
	gnuplot fib_comp.gp
	#@diff -u out scripts/expected.txt && $(call pass)
	#@scripts/verify.py
```
先將已經 `load` 過的模組釋放掉，在編譯新修改過的模組，接著編譯自製的 `client_plot.c` 檔，並載入 `fibdrv.ko` 模組，執行 `client_plot` 並導入到 `out` 後再利用 `gnuplot` 畫出其結果。

以下為基本費波那契數的計算時間：
 * Normal_Fib.png
![](https://i.imgur.com/0JWg5XX.png)

以下為基礎 Fast Doubling 的計算時間：
 * Fast_Doubling.png
![](https://i.imgur.com/oRZdP6s.png)

以下為基礎 Fast Doubling 配合 clz 的計算時間：
 * Fast_Doubling_clz.png
![](https://i.imgur.com/7g7Z0uR.png)

以下為各計算方式在 kernel space 的計算時間：
 * Compare_Fib.png
![](https://i.imgur.com/YOG4vO9.png)

由以上之圖片可以看到， kernel space 在程式執行期間所佔的時間其實是非常少的，這個結果在三種計算方式下都是正確的。除此之外，我們也可以在最後一張圖看到一般的費波那契數算法在數字變大時，其計算時間是大於 Fast Doubling 的。並且若我們使用 `__builtin_clzll()` 配合 Fast Doubling ，則可以獲得更短並且更平穩的計算時間。

### 利用 String 實做大數加法
為了實做大數的加法以突破 `long long` 的數值限制，我決定使用將數字轉換為字串以方便儲存與計算，為了方便計算，在加上一個表示字串位數的變數，並合成一個結構。
 * 結構 `BigN` 之基本表示：
```c
#define MAX 120

typedef struct _BigN {
    int digits;
    char num[MAX];
} BigN;
```

我們定義一個結構名為 `_BigN` ，其中包含一個表示字串位數的 `digits` 與實際代表字串的 `num[MAX]` ，會將字串陣列設定為 `MAX` 的巨集是因為這樣可以依據計算時所需之最大空間進行更改。以目前為例，如果需要將費波那契數列計算至第 500 個，則需要 104 個位數，因此我定義 `MAX` 為 120。

#### 實做基本加法
為了可以實做加法，我們必須將型態為 `char` 的字串轉行為 `int` 才可以進行運算，同樣的，在計算完後，也需要將 `int` 的型態轉換回 `char` ，這樣在輸出字串時才不會顯示錯誤。另外，在實做加法時需要從最低位進行對齊，而若兩字串表示方式為從最高位開始，因此兩數位數不同進行加法的姊果是錯誤的，此時便需要先將兩數進行反轉，此時最低位已經對齊，進行正確的加法後，再反轉至正常的表現方式，此時計算才算完成。
 * 字串轉型 `char_to_int()` 、 `int_to_char()` ：
```c
static inline void char_to_int(BigN *str)
{
    for (int i = 0; i < str->digits; i++) {
        *(str->num + i) -= '0';
    }
}

static inline void int_to_char(BigN *str)
{
    for (int i = 0; i < str->digits; i++) {
        *(str->num + i) += '0';
    }
}
```
在字串轉型可以看到，在 `char` 轉為 `int` 時是減去 `'0'` ，而 `int` 轉回 `char` 時則剛好相反，其原因在於字串的數字與實際的數字相差 48 ，轉換為字串後 48 即為 '0' 。

|          | 數字 0 | 字符 '0' |  ~~  | 數字 9 | 字符 '9' |
|:--------:|:------:|:--------:|:---:|:------:|:--------:|
| ASCII 碼 |   0    |    48    |  ~~  |   9    |    57    |
 * 字串反轉 `reverse()` ：
```c
static inline void reverse(BigN *str)
{
    char d = str->digits - 1;
    for (int i = 0; i < str->digits / 2; i++, d--) {
        *(str->num + i) ^= *(str->num + d);
        *(str->num + d) ^= *(str->num + i);
        *(str->num + i) ^= *(str->num + d);
    }
}
```
字串反轉的部份，則是透過連續做三次 `^` 的方式，達到兩字符交換的結果，而因為交換為一次兩個，所以只需要做總數的一半次數即反轉完成。

接著是實做真正加法與處理進位的部份，加法的實做上不複雜，但是進位需要注意的是進行進位的前提為字串為反轉的情況下完成的，以免為了進位需要挪出空間，而將所有字符都向後位移一位。
 * 字串相加 `addBigN()` ：
```c=
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
```
在程式中最為重要的內容為處理進位的 `handle_carry()` 與 第 7 行，因為加法時做完之位數需要正確才能進行下一個計算，因此這裡使用了 `? :` 判斷式找出位數較多的數字並複製其位數至 `output->digits` 。

 * 處理進位 `handle_carry()` ：
```c
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
```
處理進位的辦法為從最低位開始一一進行計算，將數字除以 `10` 的商數進位加入更高位，剩餘之餘數則為本身更新後的數值，最後再檢查最高位是否有進位，若有則需要將位數加一，並且在加上新的休止符號 `'\0'` 。

 * 整體實做方法 `fib_sequence_str()` ：
```c
static BigN fib_sequence_str(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    BigN *f = malloc(sizeof(BigN) * (k + 2));
    for(int i = 0; i < k+2; i++){
        memset(f[i].num, 0, MAX);
    }

    f[0].digits = 1;
    *(f[0].num + 0) = '0';
    *(f[0].num + 1) = 0;
    f[1].digits = 1;
    *(f[1].num + 0) = '1';
    *(f[1].num + 1) = 0;
    
    if(k < 2)
        return f[k];

    for (int i = 2; i <= k; i++) {
        addBigN_3(&f[i-2], &f[i-1], &f[i]);
    }
    return f[k];
}
```

#### 檢驗至第 500 位
設計完程式後，便可以先測試其計算至 500 位的結果：
```c
#define MAX 120

typedef struct _BigN
{
    int digits;
    char num[MAX];
} BigN;

static inline void reverse(BigN *str)
{
    int d = str->digits - 1;
    for(int i = 0; i < str->digits/2; i++, d--){
        *(str->num + i) ^= *(str->num + d);
        *(str->num + d) ^= *(str->num + i);
        *(str->num + i) ^= *(str->num + d);
    }
}

static inline void char_to_int(BigN *str)
{
    for(int i = 0; i < str->digits; i++){
        *(str->num + i) -= '0';
    }
}

static inline void int_to_char(BigN *str)
{
    for(int i = 0; i < str->digits; i++){
        *(str->num + i) += '0';
    }
}

static inline void handle_carry(BigN *str) // the string is reversed and format is int
{
    for(int i = 0; i < str->digits; i++){
        *(str->num + (i + 1)) += *(str->num + i) / 10;
        *(str->num + i) %= 10;
    }
    if(*(str->num + str->digits))
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

static BigN fib_sequence_str(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    BigN *f = malloc(sizeof(BigN) * (k + 2));
    for(int i = 0; i < k+2; i++){
        memset(f[i].num, 0, MAX);
    }

    f[0].digits = 1;
    *(f[0].num + 0) = '0';
    *(f[0].num + 1) = 0;
    f[1].digits = 1;
    *(f[1].num + 0) = '1';
    *(f[1].num + 1) = 0;
    
    if(k < 2)
        return f[k];

    for (int i = 2; i <= k; i++) {
        addBigN_3(&f[i-2], &f[i-1], &f[i]);
    }
    return f[k];
}

int main()
{
    for(long long i = 0LL; i <= 500LL; i++){
        printf("for i = %3lld, Fib(%3lld) = %s\n", i, i, fib_sequence_str(i).num);
    }
    return 0;
}
```

計算至第 500 個的結果：
```shell
for i =   0, Fib(  0) = 0
for i =   1, Fib(  1) = 1
.
.
.
for i = 499, Fib(499) = 86168291600238450732788312165664788095941068326060883324529903470149056115823592713458328176574447204501
for i = 500, Fib(500) = 139423224561697880139724382870407283950070256587697307264108962948325571622863290691557658876222521294125
```
經過檢查，計算結果完全正確。

### 載入 Linux 核心模組
撰寫完最基本的大數加法後，便可以將成功的實做方法加入 `fibdrv.c` 的檔案中。為了讓 `client.c` 可以成功輸出正確的費波那契數，必須將輸出的 `%lld` 改變成 `%s` ，並且不使用 `long long sz` ，而是改用 `char buf[]` ，並且為了可以容納足夠字串，我們將 `buf` 的容量定為 120 ，即為 `char buf[120]` 。藉由 `sz = read(fd, buf, 1)` 進入 `fibdrv.c` 修改 `buf` 的內容，再輸出回 `client.c` 即可。
 *  `client.c` 的內容：
```c
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    long long sz;

    char buf[120];
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    close(fd);
    return 0;
}
```

為了可以讓 `read(fd, buf, 1)` 可以成功修改 `buf` 的內容，我們需要修改一些內容，讓原本之 `fib_sequence()` 修改為 `fib_sequence_str()` 並加入傳入的參數 `buf` 。
 *  `fib_read()` 的內容：
```c
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    return (ssize_t) fib_sequence_str(*offset, buf);
}
```

接下來便是讓修改後的 `fib_sequence_str()` 修改 `buf` 的內容。
 *  `fib_sequence_str()` 的內容：
```c
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
    copy_to_user(buf, f[k].num, 120);
    vfree(f);
    return 1;
}
```

程式先將定義一指標 `f` 指向 `BigN` 的陣列，由於程式是在 Linux 核心中，因此無法使用一般程式中的 `malloc` 與 `free` ，這裡我們使用 `vmalloc` 與 `vfree` 。在成功計算出需要之費波那契數後，我們使用 Linux 核心的函式 `copy_to_user()` ，將 kernel space 的資料複製至 user space 。

撰寫完 Linux 核心程式碼之後，便可以使用 Makefile 測試是否有計算成功，於終端機輸入 `make check` ：
```shell
$ make check
.
.
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
 Passed [-]
f(93) fail
input: 7540113804746346429
expected: 12200160415121876738
```
輸出的 `Passed [-]` 代表經過與 `expected.txt` 的比較其內容是完全一樣的，而下一行的 `f(93) fail` 則是代表計算到第 93 個費波那契數就不正確了，其原因為 `fibdrv.c` 的 `MAX_LENGTH` 設定為 92 ，超過 92 後還數會輸入 92 ，因此後面的結果會不相同。

為了將限制解除，修改 `fibdrv.c` 的 `MAX_LENGTH` 修改為 100。
 *  `fibdrv.c` ：
```c=22
#define MAX_LENGTH 100
```
並且先不執行與 `expected.txt` 的比較，因為其超過 92 的費波那契數還是會顯示第 92 個。
 *  `Makefile` 修改：
```c
check: all
	$(MAKE) unload
	$(MAKE) load
	sudo ./client > out
	$(MAKE) unload
	#@diff -u out scripts/expected.txt && $(call pass)
	@scripts/verify.py
```

執行 `make check` ：
```shell
$ make check
.
.
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
```
輸出結果沒有顯示 fail 即為代表輸出成功，並沒有檢查到錯誤。

#### 將數字推廣至第 500 個
為了可以成功輸出第 500 個費波那契數，我們需要再次修改 `fibdrv.c` 、 `verify.py` 與 `client.c` 的限制。
 *  `client.c` 修改  `offset` ：
```c=16
    int offset = 500; /* TODO: try test something bigger than the limit */
```
 *  `fibdrv.c` 修改 `MAX_LENGTH` ：
```c=22
#define MAX_LENGTH 500
```
 *  `verify.py` 修改 `range()` ：
```python=8
for i in range(2, 501):
```

解除限制後，便可以再次測試第 500 個費波那契數是否正確。
```shell
$ make check
.
.
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
```

同樣沒有顯示 fail ，代表第 500 個費波那契數也計算成功。

### 實做字串的 Fast Doubling
基本的加法已經實做成功後，接下來便可以繼續實做 Fast Doubling 的計算，由於 Fast Doubling 除了加法外，額外需要減法、乘法、平方與乘以二，因此需要額外撰寫其他的函式：
 * 實做 Fast Doubling 的 `fib_sequence_fast_db_str()` ：
```c
static BigN fib_sequence_fast_db_str(long long k)
{
    BigN *f = malloc(sizeof(BigN) * 2);
    for(int i = 0; i < 2; i++){
        memset(f[i].num, 0, MAX);
    }
    f[0].digits = 1;
    *(f[0].num + 0) = '0';
    *(f[0].num + 1) = 0;
    f[1].digits = 1;
    *(f[1].num + 0) = '1';
    *(f[1].num + 1) = 0;
    
    if(k < 2)
        return f[k];

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
        addBigN_3(square(&temp1), square(&temp0), (f + 1)));
        if (k & what) {
            addBigN_2((f + 0), (f + 1));
            minusBigN_2_latter((f + 1), (f + 0));
        }
        what >>= 1;
    }
    return *(f + 0);
}
```

接下來為為了實做 Fast Doubling 額外增加的函式：
 * 複製內容的 `copy_data()` ：
```c
static inline void copy_data(BigN *copier, BigN *copied)
{
    for(int i = 0; i <= copied->digits; i++)
        *(copier->num + i) = *(copied->num + i);
    copier->digits = copied->digits;
}
```
 * 數字相減後並將結果覆蓋至第一個參數 `minusBigN_2_former()` ：
```c
void minusBigN_2_former(BigN *output, BigN *x) // output > x
{
    reverse(x);
    reverse(output);
    char_to_int(x);
    char_to_int(output);
    output->digits = x->digits;
    for(int i = 0; i < x->digits; i++){
        *(output->num + i) = *(output->num + i) - *(x->num + i);
    }
    handle_regroup(output);
    reverse(x);
    reverse(output);
    int_to_char(x);
    int_to_char(output);
}
```
 * 將字串乘以二的 `multiply_by_2()` ：
```c
static inline BigN *multiply_by_2(BigN *output)
{
    reverse(output);
    for(int i = 0; i < output->digits; i++){
        *(output->num + i) -= '0';
        *(output->num + i) <<= 1;
    }
    handle_carry(output);
    for(int i = 0; i < output->digits; i++){
        *(output->num + i) += '0';
    }
    reverse(output);
    return output;
}
```
 * 兩兩相乘的 `multiplication_2()` ：
```c
static inline BigN *multiplication_2(BigN * x, BigN *output)
{
    reverse(x);
    char_to_int(x);
    reverse(output);
    char_to_int(output);
    BigN *temp0 = malloc(sizeof(BigN));
    BigN *temp1 = malloc(sizeof(BigN));
    copy_data(temp0, x);
    copy_data(temp1, output);
    memset(output->num, 0, MAX);
    int length0 = temp0->digits;
    int length1 = temp1->digits;
    output->digits = output->digits > x->digits ? output->digits : x->digits;
    for(int j = 0; j < temp0->digits; j++){
        *(output->num + j) += *(temp0->num + j) * *(temp1->num + 0);
    }
    handle_carry(output);
    for(int i = 1; i < temp1->digits; i++){
        for(int j = 0; j < temp0->digits; j++){
            *(temp0->num + j) *= 10;
        }
        handle_carry(temp0);
        output->digits = output->digits > temp0->digits ? output->digits : temp0->digits;
        for(int j = 0; j < temp0->digits; j++){
            *(output->num + j) += *(temp0->num + j) * *(temp1->num + i);
            handle_carry(output);
        }
        handle_carry(output);
    }
    free(temp0);
    free(temp1);
    reverse(output);
    int_to_char(output);
    reverse(x);
    int_to_char(x);
    return output;
}
```
 * 數字相加並回傳至第三個參數的 `addBigN_3()` ：
```c
void addBigN_3(BigN *x, BigN *y, BigN *output)
{
    reverse(x);
    reverse(y);
    char_to_int(x);
    char_to_int(y);
    output->digits = x->digits > y->digits ? x->digits : y->digits;
    for(int i = 0; i < output->digits; i++){
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
```
 * 對字串開平方的 `square()` ：
```c
static inline BigN *square(BigN *side)
{
    reverse(side);
    char_to_int(side);
    BigN *temp0 = malloc(sizeof(BigN));
    BigN *temp1 = malloc(sizeof(BigN));
    copy_data(temp0, side);
    copy_data(temp1, side);
    memset(side->num, 0, MAX);
    int length = temp0->digits;
    for(int j = 0; j < temp0->digits; j++){
        *(side->num + j) += *(temp0->num + j) * *(temp1->num + 0);
        handle_carry(side);
    }
    for(int i = 1; i < length; i++){
        for(int j = 0; j < temp0->digits; j++){
            *(temp0->num + j) *= 10;
        }
        handle_carry(temp0);
        side->digits = side->digits > temp0->digits ? side->digits : temp0->digits;
        for(int j = 0; j < temp0->digits; j++){
            *(side->num + j) += *(temp0->num + j) * *(temp1->num + i);
            handle_carry(side);
        }
    }
    free(temp0);
    free(temp1);
    reverse(side);
    int_to_char(side);
    return side;
}
```
 * 兩數相加並覆蓋第二參數的 `addBigN_2()` ：
```c
void addBigN_2(BigN *x, BigN *output)
{
    reverse(x);
    char_to_int(x);
    reverse(output);
    char_to_int(output);
    output->digits = x->digits > output->digits ? x->digits : output->digits;
    for(int i = 0; i < output->digits; i++){
        *(output->num + i) = *(x->num + i) + *(output->num + i);
    }
    handle_carry(output);
    reverse(x);
    reverse(output);
    int_to_char(x);
    int_to_char(output);
}
```
 * 兩數相減並覆蓋第二個參數的 `minusBigN_2_latter()` ：
```c
void minusBigN_2_latter(BigN *x, BigN *output) // x > output
{
    reverse(x);
    reverse(output);
    char_to_int(x);
    char_to_int(output);
    output->digits = x->digits;
    for(int i = 0; i < x->digits; i++){
        *(output->num + i) = *(x->num + i) - *(output->num + i);
    }
    handle_regroup(output);
    reverse(x);
    reverse(output);
    int_to_char(x);
    int_to_char(output);
}
```

撰寫完所需之函式後，便可以測試是否輸出正確。
測試檔：
```c
int main()
{
    for(long long i = 0LL; i <= 500LL; i++){
        printf("for i = %3lld, Fib(%3lld) = %s\n", i, i, fib_sequence_fast_db_str(i).num);
        printf("for i = %3lld, Fib(%3lld) = %s\n", i, i, fib_sequence_str(i).num);
    return 0;
}
```
輸出結果：
```
for i =   0, Fib(  0) = 0
for i =   0, Fib(  0) = 0
for i =   1, Fib(  1) = 1
for i =   1, Fib(  1) = 1
.
.
for i = 499, Fib(499) = 86168291600238450732788312165664788095941068326060883324529903470149056115823592713458328176574447204501
for i = 499, Fib(499) = 86168291600238450732788312165664788095941068326060883324529903470149056115823592713458328176574447204501
for i = 500, Fib(500) = 139423224561697880139724382870407283950070256587697307264108962948325571622863290691557658876222521294125
for i = 500, Fib(500) = 139423224561697880139724382870407283950070256587697307264108962948325571622863290691557658876222521294125
```
透過與完全正確的 `fib_sequence()` 比較後，兩者完全相同，代表使用 Fast Doubling 輸出的字串是對的。

### 載入 Linux 核心模組
將撰寫完畢的程式碼複製至 `fibdrv.c` 中，並修改一些內容。
 * 實做 Fast Doubling 的 `fib_sequence_fast_db_str()` ：
```c
static long long fib_sequence_fast_db_str(long long k, const char *buf)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    BigN *f = vmalloc(sizeof(BigN) * 2);
    for(int i = 0; i < 2; i++){
        memset(f[i].num, 0, MAX);
    }
    f[0].digits = 1;
    *(f[0].num + 0) = '0';
    *(f[0].num + 1) = 0;
    f[1].digits = 1;
    *(f[1].num + 0) = '1';
    *(f[1].num + 1) = 0;
    
    if(k < 2){
        copy_to_user(buf, f[k].num, 120);
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
    copy_to_user(buf, f[0].num, 120);
    vfree(f);
    return 1;
}
```
修改的地方只有將 `malloc` 與 `free` 修改成 `vmalloc` 與 `vfree` ，並且加上 `buf` 的參數與 `copy_to_user()` 的使用。

#### 檢測大數運算結果
將限制解除至第 500 個費波那契數，並使用 `verify.py` 檢測結果：
```shell
$ make check
make -C /lib/modules/5.13.0-39-generic/build M=/home/steven/linux2022/fibdrv modules
make[1]: Entering directory '/usr/src/linux-headers-5.13.0-39-generic'
make[1]: Leaving directory '/usr/src/linux-headers-5.13.0-39-generic'
make unload
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
rmmod: ERROR: Module fibdrv is not currently loaded
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
make load
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo insmod fibdrv.ko
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
sudo ./client > out
make unload
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
```
輸出結果沒有顯示錯誤，代表輸出正確。

### 另一種計算方法
經過和同學討論後，同學提出可以使用 `unsigned long long` 進行實做，如此可以避免 `int` 與 `char` 的轉換，以及不需要一直將字串進行反轉。
目前之實做方法為定義一個結構體 `_bn` ，其中包含 `unsigned long long` 數量的 `length` ，以及真正代表數字本身的指標 `num_arr` 。並且，因為程式時常需要使用到 `unsigned long long` ，因此我使用一個 `typedef` 方便做。
 * 結構 `_bn` 與 `typedef` ：
```c
typedef unsigned long long ullong;

typedef struct _bn
{
    char length;
    unsigned long long *num_arr;
} bn;
```
結構 `bn` 會使用指標來儲存 `unsigned long long` 的原因為不知道使用者會需要使用到多大的費波那契數列，因此使用指標可以進行動態配置記憶體以因應任何位數的數字。

另外，為了方便取值與定義需要進位的值，我使用巨集先定義，這要若需要進行修改只需要修改巨集即可：
```c
#define ULL_CARRY 1000000000000000000ULL
#define RESULT_INDEX(i) *(result->num_arr + i)
```

 * 接下來為進行配置記憶體的 `create_bn()` ：
```c
static inline bn *create_bn(ullong x)
{
    bn *new = malloc(sizeof(bn));
    new->length = 1;
    new->num_arr = malloc(sizeof(ullong) * new->length);
    *(new->num_arr + 0) = x;
    return new;
}
```
函式將傳送進來 `x` 複製至新配置空間的 `*(new->num_arr + 0)` ，並回傳建立好空間的指標。另外會將 `new->length` 設定為 1 的原因為計算費波那契數時都是由開頭的 0 與 1 開始，而他們所需的 `unsigned long long` 的數量只需 1 個。

 * 釋放記憶體空間的 `free_bn()` ：
```c
static inline void free_bn(bn *x)
{
    free(x->num_arr);
    free(x);
}
```
需要先釋放指向 `unsigned long long` 的 `num_arr` 後，在釋放整個結構體的空間。

 * 計算相加的 `add_bn_2()` ：
```c
static inline bn *add_bn_2(bn *summand, bn *result)
{
    for(int i = 0; i < summand->length; i++)
        RESULT_INDEX(i) += *(summand->num_arr + i);
    handle_carry(result);
    return result;
}
```
實做的方法為先配置一個長度與較大數字相同的記憶體空間，接著先將較小的數與較大的數皆有的位數進行相加，接著在複製只有較大數字才有的位數，最後進行進位的整理再回傳。

 * 處理進位的 `handle_carry_long()` ：
```c
static inline void handle_carry_long(bn *result)
{
    for(int i = 0; i < result->length - 1; i++){
        RESULT_INDEX(i + 1) += RESULT_INDEX(i) / ULL_CARRY;
        RESULT_INDEX(i) %= ULL_CARRY;
    }
    if(RESULT_INDEX(result->length - 1) > ULL_CARRY){
        ullong *new_array = malloc(sizeof(ullong) * (result->length + 1));
        for(int i = 0; i < result->length - 1; i++)
            new_array[i] = RESULT_INDEX(i);
        new_array[result->length] = RESULT_INDEX(result->length - 1) / ULL_CARRY;
        new_array[result->length - 1] = RESULT_INDEX(result->length - 1) % ULL_CARRY;
        result->length++;
        free(result->num_arr);
        result->num_arr = new_array;
    }
}
```
將傳進來 `result` 分別進行取餘數與進位，並檢查最高位的 `unsigned long long` 之值是否大於 `ULL_CARRY` ，若是則代表最高位已經無法容納目前之數字，需要增加一位的空間。因此先配置好記憶體 `new_array` ，並複製除最高位之其他位數，並在處理完進位後在複製至新的空間，將舊的空間釋放再使用新的空間。

 * 計算相減的 `sub_bn_2()` ：
```c
 // result = result - small
static inline bn *sub_bn_2(bn *small, bn *result)
{
    for(int i = 0; i < small->length; i++)
        RESULT_INDEX(i) = RESULT_INDEX(i) - *(small->num_arr + i);
    for(int i = 0; i < small->length; i++){
        if(RESULT_INDEX(i) > ULL_CARRY){
            RESULT_INDEX(i + 1) -= 1;
            RESULT_INDEX(i) += ULL_CARRY;
        }
    }
    int new_length = result->length - 1;
    if(!RESULT_INDEX(new_length)){
        while(!RESULT_INDEX(new_length))
            new_length--;
        new_length++;
        ullong *new_array = malloc(sizeof(ullong) * new_length);
        for(int i = 0; i < new_length; i++)
            *(new_array + i) = RESULT_INDEX(i);
        free(result->num_arr);
        result->num_arr = new_array;
        result->length = new_length;
    }
    return result;
}
```
其中需要注意的是，若是相減之後高位數的 `unsigned long long` 之值為 0 ，則代表需要重新分配空間。

 * 複製數值的 `copy_data_long()` ：
```c
static inline bn *copy_data(bn *result)
{
    bn *dup = malloc(sizeof(bn));
    dup->length = result->length;
    dup->num_arr = malloc(sizeof(ullong) * result->length);
    for(int i = 0; i < result->length; i++)
        *(dup->num_arr + i) = RESULT_INDEX(i);
    return dup;
}
```

 * 列印數字以方便確認的 `print_number()` ：
```c
void print_number(bn *x)
{
    printf("%llu", *(x->num_arr + x->length - 1));
    for(char i = x->length - 2; i >= 0; i--){
        printf("%018llu", *(x->num_arr + i));
    }
    printf("\n");
    free_bn(x);
}
```
需要注意的是，由於數值是分別存放於各個 `unsigned long long` 中，因此會遇到中間之數值過小導致列印時顯示錯誤，這時需要在 `%llu` 中間加上 `018` ，變成 `%018llu` 其中 18 的意思為總共需要印出 18 個位數，但是因為有些數值過小導致數字前面的 0 並不會印出來，加上 0 就可以顯示出 0 了。

 * 實作主程式 `fib_long_iter()` ：
```c
static bn *fib_long_iter(int i, char *buf)
{
    bn *a, *b;
    a = create_bn(0ULL);
    b = create_bn(1ULL);
    if(i == 0){
        free_bn(b);
        return a;
    }
    if(i == 1 || i == 2){
        free_bn(a);
        return b;
    }
    for(int j = 0; j < i - 1; j++){
        add_bn_2(a, b);
        bn *tmp = copy_data_long(b);
        sub_bn_2(a, tmp);
        free_bn(a);
        a = tmp;
    }
    free_bn(a);
    return a;
}
```

#### 驗證是否成功
為了測試是否可以使用其計算費波那契數，可以測試其 0 ~ 500 之數值：
 * 主要測試程式 `main()` ：
```c
int main()
{
    for(int i = 0; i <= 500; i++){
        printf("For i = %3d: f(%3d) = ", i, i);
        print_number(fib_long_iter(i));
    }
    return 0;
}
```

測試結果：
```shell
$ make long
.
.
.
For i = 498: f(498) = 53254932961459429406936070704742495854129188261636423939579059478176515507039697978099330699648074089624
For i = 499: f(499) = 86168291600238450732788312165664788095941068326060883324529903470149056115823592713458328176574447204501
For i = 500: f(500) = 139423224561697880139724382870407283950070256587697307264108962948325571622863290691557658876222521294125
```

經過檢驗，本程式測試至第 5000 個也沒有問題：
```shell
$ make long
.
.
For i = 5000: f(5000) = 3878968454388325633701916308325905312082127714646245106160597214895550139044037097010822916462210669479293452858882973813483102008954982940361430156911478938364216563944106910214505634133706558656238254656700712525929903854933813928836378347518908762970712033337052923107693008518093849801803847813996748881765554653788291644268912980384613778969021502293082475666346224923071883324803280375039130352903304505842701147635242270210934637699104006714174883298422891491273104054328753298044273676822977244987749874555691907703880637046832794811358973739993110106219308149018570815397854379195305617510761053075688783766033667355445258844886241619210553457493675897849027988234351023599844663934853256411952221859563060475364645470760330902420806382584929156452876291575759142343809142302917491088984155209854432486594079793571316841692868039545309545388698114665082066862897420639323438488465240988742395873801976993820317174208932265468879364002630797780058759129671389634214252579116872755600360311370547754724604639987588046985178408674382863125
```

### 載入 Linux 核心模組
為了將程式碼載入 Linux 核心，必須將 `malloc()` 與 `free()` 更改為 `vmalloc()` 與 `vfree()` 。還必須使用 `copy_to_user()` 將 Kernel 端的資料複製至 user 端。

 * 於 Kernel 端的主程式 `fib_long_iter()` ：
```c
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
        add_bn_2(a, b);
        bn *temp = copy_data_long(b);
        sub_bn_2(a, temp);
        free_bn(a);
        a = temp;
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
```
程式中會使用到 `snprintf()` 的原因為需要將原本儲存在 `bn->num_arr` 中的 `unsigned long long` 陣列之值複製至陣列 `tmp` ，再使用 `copy_to_user()` 將 `tmp` 之值複製至 `buf` 。

#### 驗證輸出結果
 * 於終端機輸入 `make check` ：
```shell
$ make check

make -C /lib/modules/5.13.0-39-generic/build M=/home/steven/linux2022/fibdrv modules
make[1]: Entering directory '/usr/src/linux-headers-5.13.0-39-generic'
make[1]: Leaving directory '/usr/src/linux-headers-5.13.0-39-generic'
make unload
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
[sudo] password for steven: 
rmmod: ERROR: Module fibdrv is not currently loaded
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
make load
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo insmod fibdrv.ko
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
sudo ./client > out
make unload
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
diff -u out scripts/expected.txt && env printf "\e[32;01m Passed [-]\e[0m\n"
 Passed [-]
scripts/verify.py
f(93) fail
input: 7540113804746346429
expected: 12200160415121876738
```

可以看到輸出之 `out` 檔與 `expected.txt` 之內容完全相同，由於尚未修改極限值所以到第 93 個費波那契數時會產生錯誤。

這次將與 `expected.txt` 的比較註解掉，並修改數值極限以單純比較至第 500 個費波那數是否正確。
 * 再次測試：
```shell
$ make check

make -C /lib/modules/5.13.0-39-generic/build M=/home/steven/linux2022/fibdrv modules
make[1]: Entering directory '/usr/src/linux-headers-5.13.0-39-generic'
make[1]: Leaving directory '/usr/src/linux-headers-5.13.0-39-generic'
make unload
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
rmmod: ERROR: Module fibdrv is not currently loaded
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
make load
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo insmod fibdrv.ko
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
sudo ./client > out
make unload
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
#diff -u out scripts/expected.txt && env printf "\e[32;01m Passed [-]\e[0m\n"
scripts/verify.py
```
沒有出現錯誤，代表前 500 個費波那契數是沒有問題的。

### 加上 Fast doubling 與 clz() 
使用 `unsigned long long` 實做基本加法成功之後，我們可以再進一步使用 Fast doubling 與 `clz()` 幫助加快計算。

 * 以下為主要之實做程式 `fib_sequence_fast_db_long()` ：
```c
static bn *fib_sequence_fast_db_long(long long k)
{
    bn *f0 = create_bn(0ULL);
    bn *f1 = create_bn(1ULL);
    
    if(k == 0){
        free_bn(f1);
        return f0;
    }
    if(k < 3){
        free_bn(f0);
        return f1;
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
            bn *tmp = copy_data_long(f1);
            sub_bn_2(f0, tmp);
            free_bn(f0);
            f0 = tmp;
        }
        what >>= 1;
        free_bn(temp0);
        free_bn(temp1);
    }
    free_bn(f1);
    return f0;
}
```

 * 與使用 `clz()` 的 `fib_sequence_fast_db_long_clz()` ：
```c
static bn *fib_sequence_fast_db_long(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    bn *f0 = create_bn(0ULL);
    bn *f1 = create_bn(1ULL);
    
    if(k == 0){
        free_bn(f1);
        return f0;
    }
    if(k < 3){
        free_bn(f0);
        return f1;
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
            bn *tmp = copy_data_long(f1);
            sub_bn_2(f0, tmp);
            free_bn(f0);
            f0 = tmp;
        }
        what >>= 1;
        free_bn(temp0);
        free_bn(temp1);
    }
    free_bn(f1);
    return f0;
}
```
其主要差異只有在 `clz()` 的使用與否，其餘實做皆為相同的。

其餘程式之講解：
 * 將數值乘以 2 的 `multiply_by2()` ：
```c
static inline bn *multiply_by2(bn *result)
{
    for(int i = 0; i < result->length; i++)
        RESULT_INDEX(i) <<= 1;
    handle_carry(result);
    return result;
}
```

 * 實做開平方的 `square_long()` ：
```c
static inline bn *square(bn *result)
{
    bn *dup = copy_data(result);
    bn *tmp = copy_data(result);
    free(result->num_arr);
    result->length <<= 1;
    result->num_arr = malloc(sizeof(ullong) * result->length--);
    for(int i = 0; i < result->length; i++)
        RESULT_INDEX(i) = 0ULL;
    for(int i = 0; i < tmp->length; i++)
        RESULT_INDEX(i) = *(tmp->num_arr + i) * *(dup->num_arr + 0);
    handle_carry(result);
    for(int i = 1; i < dup->length; i++){
        for(int j = 0; j < tmp->length; j++)
            *(tmp->num_arr + j) *= ULL_CARRY;
        handle_carry(tmp);
        for(int j = 0; j < tmp->length; j++)
            RESULT_INDEX(j) += *(tmp->num_arr + j) * *(dup->num_arr + i);
        handle_carry(result);
    }
    free_bn(dup);
    free_bn(tmp);
    return result;
}
```

 * 實做相乘的 `multiplication()` ：
```c
static inline bn *multiplication(bn *multiplicand, bn *result)
{
    bn *dup = copy_data(result);
    bn *tmp = copy_data(multiplicand);
    free(result->num_arr);
    result->length += multiplicand->length;
    result->num_arr = malloc(sizeof(ullong) * result->length--);
    for(int i = 0; i < result->length; i++)
        RESULT_INDEX(i) = 0ULL;
    for(int i = 0; i < tmp->length; i++)
        RESULT_INDEX(i) = *(tmp->num_arr + i) * *(dup->num_arr + 0);
    handle_carry(result);
    for(int i = 1; i < dup->length; i++){
        for(int j = 0; j < tmp->length; j++)
            *(tmp->num_arr + j) *= ULL_CARRY;
        handle_carry(tmp);
        for(int j = 0; j < tmp->length; j++)
            RESULT_INDEX(j) += *(tmp->num_arr + j) * *(dup->num_arr + i);
        handle_carry(result);
    }
    free_bn(dup);
    free_bn(tmp);
    return result;
}
```

### 載入 Linux 核心模組
同樣的需要修改 `malloc()` 與 `free()` ，以及使用 `snprintf()` 和 `copy_to_user()` 。

 * 核心中的 `fib_fast_db_long_clz()` ：
```c
static long long fib_fast_db_long_clz(long long k, const char *buf)
{
    bn *f0 = create_bn(0ULL);
    bn *f1 = create_bn(1ULL);
    char tmp[120];
    
    if(k == 0){
        free_bn(f1);
        snprintf(tmp, ARRAY_LENGTH, "%llu", *(f0->num_arr + f0->length - 1));
        copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
        free_bn(f0);
        return 1;
    }
    if(k < 3){
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
            bn *tmp = copy_data_long(f1);
            sub_bn_2(f0, tmp);
            free_bn(f0);
            f0 = tmp;
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
        snprintf(tmp + l + w, ARRAY_LENGTH, "%018llu", *(f0->num_arr + i));
        w += 18;
    }
    copy_to_user((void *) buf, tmp, ARRAY_LENGTH);
    free_bn(f0);
    return 1;
}
```

#### 檢測結果
 * 於終端機輸入 `make check` ：
```shell
$ make check
make -C /lib/modules/5.13.0-39-generic/build M=/home/steven/linux2022/fibdrv modules
make[1]: Entering directory '/usr/src/linux-headers-5.13.0-39-generic'
make[1]: Leaving directory '/usr/src/linux-headers-5.13.0-39-generic'
make unload
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
rmmod: ERROR: Module fibdrv is not currently loaded
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
make load
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo insmod fibdrv.ko
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
sudo ./client > out
make unload
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
diff -u out scripts/expected.txt && env printf "\e[32;01m Passed [-]\e[0m\n"
 Passed [-]
scripts/verify.py
f(93) fail
input: 7540113804746346429
expected: 12200160415121876738
```

同樣需要修改限制至第 500 個費波那契數。
 * 修改限制後再次輸入 `make check` ：
```shell
$ make check
make -C /lib/modules/5.13.0-39-generic/build M=/home/steven/linux2022/fibdrv modules
make[1]: Entering directory '/usr/src/linux-headers-5.13.0-39-generic'
make[1]: Leaving directory '/usr/src/linux-headers-5.13.0-39-generic'
make unload
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
rmmod: ERROR: Module fibdrv is not currently loaded
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
make load
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo insmod fibdrv.ko
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
sudo ./client > out
make unload
make[1]: Entering directory '/home/steven/linux2022/fibdrv'
sudo rmmod fibdrv || true >/dev/null
make[1]: Leaving directory '/home/steven/linux2022/fibdrv'
#diff -u out scripts/expected.txt && env printf "\e[32;01m Passed [-]\e[0m\n"
scripts/verify.py
```
代表前 500 個費波那契數也沒有錯誤。

## 檢查記憶體遺失（Memory Loss）
由於在 Kernel 端執行計算若數值過大會造成電腦當機的情形，起初懷疑是因為呼叫 function 的 stack 高度過高導致 stack smash detected 。之後發現若是 stack 長太高 Kernel 會自行中止程式並回傳 `*** stack smashing detected ***: terminated` 。因此懷疑是記憶體遺失造成的。

![](https://i.imgur.com/7koN5iP.png)

### 使用 Valgrind 檢查記憶體遺失
為了使用 Valgrind 檢查記憶體缺失，需要先利用 gcc 編譯至 valgrind 能夠檢查的執行檔，因此需要在編一時加上參數 `-g` ：
```shell
$ gcc -g -o testing_fd testing_fd.c
```

接著檢查 Valgrind 是否有安裝：
```shell
$ valgrind --version
```

若已經安裝 valgrind ，則可以輸入：
```shell
$ valgrind ./testing_fd
```

若是需要較多的資訊可以輸入：
```shell
$ valgrind --leak-check=full --show-leak-kinds=all --verbose ./testing_fd
```

果然檢查到記憶體遺失：

```
==39738== LEAK SUMMARY:
==39738==    definitely lost: 48,024 bytes in 698 blocks
==39738==    indirectly lost: 40 bytes in 1 block
==39738==      possibly lost: 0 bytes in 0 blocks
==39738==    still reachable: 0 bytes in 0 blocks
==39738==         suppressed: 0 bytes in 0 blocks
==39738==
==39738== ERROR SUMMARY: 2 errors from 2 contexts (suppressed: 0 from 0)
```

經過修改之後，再次檢查可以看到已無記憶體遺失：

```
==41237== HEAP SUMMARY:
==41237==     in use at exit: 0 bytes in 0 blocks
==41237==   total heap usage: 1,513 allocs, 1,513 frees, 59.800 bytes allocated
==41237==
==41237== All heap blocks were freed -- no leaks are possible
==41237==
==41237== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

## 比較執行速度
在確認完計算費波那契數函數之正確性之與確保安全的使用記憶體後，便可比較各方法之計算時間。

首先比較前 1000 位費波那契數的計算速度，可以發現使用迭代的算法的時間複雜度為 $O(N)$ ，並且其他使用 Fast Doubling 的時間複雜度為 $O(log(N))$ ：

![](https://i.imgur.com/N5OxRgO.png)


由於在計算單次的時間數據會抖動的非常厲害，因此我們採用統計的方式，分別各計算 1000 次，再將計算結果排序後，去除數據的極端值 5 % ，保留中間數據的 95 % ，最後再相加取平均：

```c
int comp (const void * elem1, const void * elem2) 
{
    int f = *((int*)elem1);
    int s = *((int*)elem2);
    if (f > s) return  1;
    if (f < s) return -1;
    return 0;
}

int main()
{
    long long sz;

    char buf[300];
    int offset = 1000; /* TODO: try test something bigger than the limit */
    struct timespec start, end;
    long long total[1000];

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        for (int type = 6; type < 9; type++) {
            for(int j = 0; j < 1000; j++){
                clock_gettime(CLOCK_MONOTONIC, &start);
                sz = write(fd, buf, type);
                clock_gettime(CLOCK_MONOTONIC, &end);
                total[j] = sz;
            }
            qsort(total, 1000, sizeof(long long), comp);
            long long time = 0LL;
            for(int num = 30; num < 980; num++){
                time += total[num];
            }
            time /= 950;
            printf("%lld ", time);
        }
        printf("\n");
    }

    close(fd);
    return 0;
}
```

可以得到更加乾淨的比較結果：

![](https://i.imgur.com/KIuolKK.png)

可以看到經過統計移除極端值後，圖表滑順許多。

接下來會以 `long_fast_doubling_clz` 進行實做的加速：

## 改善大數實做 (Fast Doubling) 的效率

### 修改 `vmalloc` 至 `kmalloc` 
在經過與[ KYG-yaya573142 ](https://hackmd.io/@KYWeng/rkGdultSU)的實做速度進行比較之後，發現實作的速度相差了一個數量級，經過比較發現不是因為演算法的不同導致的。因此改使用 `kmalloc` 取代原本的 `vmalloc` 。

![](https://i.imgur.com/c9t6Xxj.png)

可以發現 `kmalloc` 的實做速度整整快了原本的 `vmalloc` 十倍，接下來會探討其速度會相差這麼多的原因。

經過上網查詢資料之後，可以得到以下結論：
> Kmalloc() is use to allocate memory requested from the kernel.The memory allocated is returned by the API and it is physically as well virtually contiguous.Generally we need this physically contiguous memory when we have to implement some driver which will be used to interface some hardware or device.
> .
> Vmalloc() is used to allocate memory from the kernel.The memory allocated is virtually contiguous but not physically contiguous.
> 
> 參考資料：https://www.emblogic.com/blog/10/difference-between-kmalloc-and-vmalloc/

由上述的敘述可以知道 `kmalloc()` 所配置的記憶體為實體且虛擬連續的，因此在配置或是使用時所需時間較短。但是因為需要完全連續的實體記憶體，所以其可配置的最大空間是較小的 (通常限制的大小為一個 page size) 。

而 `vmalloc()` 所配置的記憶體為虛擬連續，實體則不連續，因此配置與使用所需時間較長，也因為不需要實體連續的記憶體，所以可配置的最大空間較大。

### 減少大數運算的成本
接下來會以 perf 分析函式的熱點，在逐步改善大數運算的效能。

#### 測試環境
```shell
$ cat /proc/version
Linux version 5.13.0-40-generic (buildd@ubuntu) 
(gcc (Ubuntu 9.4.0-1ubuntu1~20.04.1) 9.4.0, 
GNU ld (GNU Binutils for Ubuntu) 2.34)

$ cat /etc/os-release
NAME="Ubuntu"
VERSION="20.04.4 LTS (Focal Fossa)"
...
```
 * 在 user space 測量
 * compiler flag : `-O2` 、 `-g` 、 `-fno-omit-frame-pointer` 

#### 運算成本



先以 `perf stat` 分析目前實作的效能，作為後續比對的基準
```
 Performance counter stats for './testing_fd' (10 runs):

         4384,1610      cycles                                                        ( +-  0.37% )
       1,0622,7381      instructions              #    2.42  insn per cycle           ( +-  0.01% )
            1,0251      cache-misses              #   42.717 % of all cache refs      ( +-  6.37% )
            2,3999      cache-references                                              ( +-  2.77% )
         1766,5506      branch-instructions                                           ( +-  0.01% )
            2,5695      branch-misses             #    0.15% of all branches          ( +-  6.07% )

          0.013888 +- 0.000538 seconds time elapsed  ( +-  3.88% )
```

接下來使用 `perf record` 量測 call graph (有省略部分內容來提升可讀性)
```shell
$ sudo perf record -g --call-graph dwarf ./testing_fd
$ sudo perf report --stdio -g graph,0.5,caller
```

```
    91.13%    58.85%  testing_fd  testing_fd         [.] main
            |          
            |--58.85%--_start
            |          __libc_start_main
            |          main
            |          fib_fast_db_long_clz_v0 (inlined)
            |          |          
            |          |--39.57%--square_long_v0 (inlined)
            |          |          |          
            |          |          |--31.63%--handle_carry_long_v0 (inlined)
            |          |          |          
            |          |           --1.61%--free_bn_v0 (inlined)
            |          |          
            |          |--16.27%--multiplication_v0 (inlined)
            |          |          |          
            |          |           --14.65%--handle_carry_long_v0 (inlined)
            |          |          
            |           --3.01%--sub_bn_2_v0 (inlined)
```


可以看到程式碼中 `square_long_v0()` 所佔用的時間最多，其中 `handle_carry_long_v0()` 又佔了大多數。

初版的實作效能如下：

![](https://i.imgur.com/yHNaBkT.png)

#### 改進 1 - 更改進位方式
由於透過 `perf` 觀察到 `handle_carry_long()` 所花費的時間最多，因此以改進這方面的程式效能為優先。經過觀察程式碼發現乘法的實做中會需要不斷乘以 `ULL_CARRY` ，再做進位。但是其實可以透過複製至下一位的方式，不做乘法，減少不必要的運算。

##### 改進結果
可以看到 `handle_carry_long()` 所耗費的時間減少非常多，也因此得到相當不錯的效能。
```
 Performance counter stats for './testing_fd' (10 runs):

         3503,0871      cycles                                                        ( +-  0.35% )
         9138,0792      instructions              #    2.61  insn per cycle           ( +-  0.01% )
              6472      cache-misses              #   32.240 % of all cache refs      ( +- 13.50% )
            2,0076      cache-references                                              ( +-  4.07% )
         1621,0249      branch-instructions                                           ( +-  0.01% )
            2,2377      branch-misses             #    0.14% of all branches          ( +-  7.81% )

          0.010646 +- 0.000459 seconds time elapsed  ( +-  4.31% )
```

```
    90.04%    50.82%  testing_fd  testing_fd         [.] main
            |          
            |--50.82%--_start
            |          __libc_start_main
            |          main
            |          fib_fast_db_long_clz_v1 (inlined)
            |          |          
            |          |--45.61%--square_long_v1 (inlined)
            |          |          |          
            |          |          |--37.12%--handle_carry_long_v1 (inlined)
            |          |          |          
            |          |           --2.68%--copy_data_long_v1 (inlined)
            |          |          
            |          |--2.74%--multiply_by2_v1 (inlined)
            |          |          handle_carry_long_v1 (inlined)
            |          |          
            |           --2.47%--multiplication_v1 (inlined)
            |                     handle_carry_long_v1 (inlined)
            |          
             --39.22%--main
                       fib_fast_db_long_clz_v1 (inlined)
                       |          
                       |--23.94%--square_long_v1 (inlined)
                       |          |          
                       |          |--5.98%--carry_v1 (inlined)
                       |          |          __GI___libc_malloc (inlined)
                       |          |          |          
                       |          |           --2.78%--tcache_get (inlined)
                       |          |          
                       |          |--5.72%--_int_free
                       |          |          
                       |          |--4.09%--__GI___libc_free (inlined)
                       |          |          
                       |          |--2.81%--handle_carry_long_v1 (inlined)
                       |          |          _int_free
                       |          |          
                       |          |--2.77%--free_bn_v1 (inlined)
                       |          |          _int_free
                       |          |          
                       |           --2.57%--copy_data_long_v1 (inlined)
                       |                     __GI___libc_malloc (inlined)
```

改進結果：

![](https://i.imgur.com/DFbSarE.png)
