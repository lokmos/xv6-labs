# Task1

理解一点RISC-V汇编是很重要的，你应该在6.004中接触过。xv6仓库中有一个文件***user/call.c***。执行`make fs.img`编译它，并在***user/call.asm***中生成可读的汇编版本。

阅读***call.asm***中函数`g`、`f`和`main`的代码。RISC-V的使用手册在[参考页](https://pdos.csail.mit.edu/6.828/2020/reference.html)上。以下是您应该回答的一些问题（将答案存储在***answers-traps.txt***文件中）：

1. 哪些寄存器保存函数的参数？例如，在`main`对`printf`的调用中，哪个寄存器保存13？
2. `main`的汇编代码中对函数`f`的调用在哪里？对`g`的调用在哪里(提示：编译器可能会将函数内联）
3. `printf`函数位于哪个地址？
4. 在`main`中`printf`的`jalr`之后的寄存器`ra`中有什么值？
5. 运行以下代码。

```c
unsigned int i = 0x00646c72;
printf("H%x Wo%s", 57616, &i);
```

程序的输出是什么？这是将字节映射到字符的[ASCII码表](http://web.cs.mun.ca/~michael/c/ascii-table.html)。

输出取决于RISC-V小端存储的事实。如果RISC-V是大端存储，为了得到相同的输出，你会把`i`设置成什么？是否需要将`57616`更改为其他值？

[这里有一个小端和大端存储的描述](http://www.webopedia.com/TERM/b/big_endian.html)和一个[更异想天开的描述](http://www.networksorcery.com/enp/ien/ien137.txt)。

1. 在下面的代码中，“`y=`”之后将打印什么(注：答案不是一个特定的值）？为什么会发生这种情况？

```c
printf("x=%d y=%d", 3);
```

## 回答

```
Q: 哪些寄存器存储了函数调用的参数？举个例子，main 调用 printf 的时候，13 被存在了哪个寄存器中？
A: a0-a7; a2;

Q: main 中调用函数 f 对应的汇编代码在哪？对 g 的调用呢？ (提示：编译器有可能会内链(inline)一些函数)
A: 没有这样的代码。 g(x) 被内链到 f(x) 中，然后 f(x) 又被进一步内链到 main() 中

Q: printf 函数所在的地址是？
A: 0x0000000000000628, main 中使用 pc 相对寻址来计算得到这个地址。

Q: 在 main 中 jalr 跳转到 printf 之后，ra 的值是什么？
A: 0x0000000000000038, jalr 指令的下一条汇编指令的地址。

Q: 运行下面的代码

	unsigned int i = 0x00646c72;
	printf("H%x Wo%s", 57616, &i);      

输出是什么？
如果 RISC-V 是大端序的，要实现同样的效果，需要将 i 设置为什么？需要将 57616 修改为别的值吗？
A: "He110 World"; 0x726c6400; 不需要，57616 的十六进制是 110，无论端序（十六进制和内存中的表示不是同个概念）

Q: 在下面的代码中，'y=' 之后会答应什么？ (note: 答案不是一个具体的值) 为什么?

	printf("x=%d y=%d", 3);

A: 输出的是一个受调用前的代码影响的“随机”的值。因为 printf 尝试读的参数数量比提供的参数数量多。
第二个参数 `3` 通过 a1 传递，而第三个参数对应的寄存器 a2 在调用前不会被设置为任何具体的值，而是会
包含调用发生前的任何已经在里面的值。
```



# Task2

回溯(Backtrace)通常对于调试很有用：它是一个存放于栈上用于指示错误发生位置的函数调用列表。

在***kernel/printf.c***中实现名为`backtrace()`的函数。在`sys_sleep`中插入一个对此函数的调用，然后运行`bttest`，它将会调用`sys_sleep`。你的输出应该如下所示：

```bash
backtrace:
0x0000000080002cda
0x0000000080002bb6
0x0000000080002898
```

## 提示

- 在***kernel/defs.h***中添加`backtrace`的原型，那样你就能在`sys_sleep`中引用`backtrace`
- GCC编译器将当前正在执行的函数的帧指针保存在`s0`寄存器，将下面的函数添加到***kernel/riscv.h***

```c
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x) );
  return x;
}
```

 并在`backtrace`中调用此函数来读取当前的帧指针。这个函数使用[内联汇编](https://gcc.gnu.org/onlinedocs/gcc/Using-Assembly-Language-with-C.html)来读取`s0`

- 这个[课堂笔记](https://pdos.csail.mit.edu/6.828/2020/lec/l-riscv-slides.pdf)中有张栈帧布局图。注意返回地址位于栈帧帧指针的固定偏移(-8)位置，并且保存的帧指针位于帧指针的固定偏移(-16)位置

![img](assets/p2.png)

- XV6在内核中以页面对齐的地址为每个栈分配一个页面。你可以通过`PGROUNDDOWN(fp)`和`PGROUNDUP(fp)`（参见***kernel/riscv.h***）来计算栈页面的顶部和底部地址。这些数字对于`backtrace`终止循环是有帮助的。

一旦你的`backtrace`能够运行，就在***kernel/printf.c***的`panic`中调用它，那样你就可以在`panic`发生时看到内核的`backtrace`。



## 实现

- 按照提示，在`defs.h`和`riscv.h`中添加声明和函数

- 实现`backtrace`

  - fp 指向当前栈帧的开始地址，sp 指向当前栈帧的结束地址。 （栈从高地址往低地址生长，所以 fp 虽然是帧开始地址，但是地址比 sp 高）

  - 栈帧中从高到低第一个 8 字节 `fp-8` 是 return address，也就是当前调用层应该返回到的地址。
    栈帧中从高到低第二个 8 字节 `fp-16` 是 previous address，指向上一层栈帧的 fp 开始地址。剩下的为保存的寄存器、局部变量等。一个栈帧的大小不固定，但是至少 16 字节。

  - 在 xv6 中，使用一个页来存储栈，如果 fp 已经到达栈页的上界，则说明已经到达栈底。

  - 扩张栈需要将地址 -16， 回收栈则为 +16

  - ```c
    // kernel/printf.c
    void backtrace() {
      uint64 fp = r_fp();
      while(fp != PGROUNDUP(fp)) { // 如果已经到达栈底
        uint64 ra = *(uint64*)(fp - 8); // return address
        printf("%p\n", ra);
        fp = *(uint64*)(fp - 16); // previous fp
      }
    }
    ```

- 在`sys_sleep`开始时调用一次`backtrace`

  - ```c
    // kernel/sysproc.c
    uint64
    sys_sleep(void)
    {
      int n;
      uint ticks0;
    
      backtrace(); 
      // ...
    ```

## 结果

![image-20240511172813642](assets/image-20240511172813642.png)



# Task3

- 在这个练习中你将向XV6添加一个特性，在进程使用CPU的时间内，XV6定期向进程发出警报。这对于那些希望限制CPU时间消耗的受计算限制的进程，或者对于那些计算的同时执行某些周期性操作的进程可能很有用。更普遍的来说，你将实现用户级中断/故障处理程序的一种初级形式。例如，你可以在应用程序中使用类似的一些东西处理页面故障。如果你的解决方案通过了`alarmtest`和`usertests`就是正确的。

- 你应当添加一个新的`sigalarm(interval, handler)`系统调用，如果一个程序调用了`sigalarm(n, fn)`，那么每当程序消耗了CPU时间达到n个“滴答”，内核应当使应用程序函数`fn`被调用。当`fn`返回时，应用应当在它离开的地方恢复执行。在XV6中，一个滴答是一段相当任意的时间单元，取决于硬件计时器生成中断的频率。如果一个程序调用了`sigalarm(0, 0)`，系统应当停止生成周期性的报警调用。

- 你将在XV6的存储库中找到名为***user/alarmtest.c\***的文件。将其添加到***Makefile\***。注意：你必须添加了`sigalarm`和`sigreturn`系统调用后才能正确编译（往下看）。

- `alarmtest`在`test0`中调用了`sigalarm(2, periodic)`来要求内核每隔两个滴答强制调用`periodic()`，然后旋转一段时间。你可以在***user/alarmtest.asm\***中看到`alarmtest`的汇编代码，这或许会便于调试。当`alarmtest`产生如下输出并且`usertests`也能正常运行时，你的方案就是正确的：

```bash
$ alarmtest
test0 start
........alarm!
test0 passed
test1 start
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
test1 passed
test2 start
................alarm!
test2 passed
$ usertests
...
ALL TESTS PASSED
$
```

- 当你完成后，你的方案也许仅有几行代码，但如何正确运行是一个棘手的问题。我们将使用原始存储库中的***alarmtest.c***版本测试您的代码。你可以修改***alarmtest.c***来帮助调试，但是要确保原来的`alarmtest`显示所有的测试都通过了。



## 提示

- 您需要修改***Makefile\***以使***alarmtest.c\***被编译为xv6用户程序。
- 放入***user/user.h\***的正确声明是：

```c
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
```

- 更新***user/usys.pl\***（此文件生成***user/usys.S\***）、***kernel/syscall.h\***和***kernel/syscall.c\***以允许`alarmtest`调用`sigalarm`和`sigreturn`系统调用。
- 目前来说，你的`sys_sigreturn`系统调用返回应该是零。
- 你的`sys_sigalarm()`应该将报警间隔和指向处理程序函数的指针存储在`struct proc`的新字段中（位于***kernel/proc.h\***）。
- 你也需要在`struct proc`新增一个新字段。用于跟踪自上一次调用（或直到下一次调用）到进程的报警处理程序间经历了多少滴答；您可以在***proc.c\***的`allocproc()`中初始化`proc`字段。
- 每一个滴答声，硬件时钟就会强制一个中断，这个中断在***kernel/trap.c\***中的`usertrap()`中处理。
- 如果产生了计时器中断，您只想操纵进程的报警滴答；你需要写类似下面的代码

```c
if(which_dev == 2) ...
```

- 仅当进程有未完成的计时器时才调用报警函数。请注意，用户报警函数的地址可能是0（例如，在***user/alarmtest.asm\***中，`periodic`位于地址0）。
- 您需要修改`usertrap()`，以便当进程的报警间隔期满时，用户进程执行处理程序函数。当RISC-V上的陷阱返回到用户空间时，什么决定了用户空间代码恢复执行的指令地址？
- 如果您告诉qemu只使用一个CPU，那么使用gdb查看陷阱会更容易，这可以通过运行

```bash
make CPUS=1 qemu-gdb
```

- 如果`alarmtest`打印“alarm!”，则您已成功。

- 您的解决方案将要求您保存和恢复寄存器——您需要保存和恢复哪些寄存器才能正确恢复中断的代码？(提示：会有很多）
- 当计时器关闭时，让`usertrap`在`struct proc`中保存足够的状态，以使`sigreturn`可以正确返回中断的用户代码。
- 防止对处理程序的重复调用——如果处理程序还没有返回，内核就不应该再次调用它。`test2`测试这个。
- 一旦通过`test0`、`test1`和`test2`，就运行`usertests`以确保没有破坏内核的任何其他部分。



## 实现

- 首先把`sigalarm`和`sigreturn`添加系统调用

- 在`proc`结构体中，增加 alarm 所需字段

  - alarm_interval：时钟周期，0 为禁用

  - alarm_handler：时钟回调处理函数

  - alarm_ticks：下一次时钟响起前还剩下的 ticks 数

  - alarm_trapframe：时钟中断时刻的 trapframe，用于中断处理完成后恢复原程序的正常执行

  - alarm_goingoff：是否已经有一个时钟回调正在执行且还未返回

  - ```c
    // kernel/proc.h  
    int alarm_interval;          
    void(*alarm_handler)();      
    int alarm_ticks;             
    struct trapframe *alarm_trapframe;  
    int alarm_goingoff;  

- 实现`sigalarm`和`sigreturn`

  - `sigalarm`设置`proc`中alarm相关的参数，通过用户给的参数传递

  - `sigreturn`通过`proc`中的陷阱帧恢复到时钟中断前的状态

  - ```c
    // kernel/sysproc.c
    uint64 sys_sigalarm(void) {
      int n;
      uint64 fn;
      if(argint(0, &n) < 0)
        return -1;
      if(argaddr(1, &fn) < 0)
        return -1;
      
      return sigalarm(n, (void(*)())(fn));
    }
    
    uint64 sys_sigreturn(void) {
    	return sigreturn();
    }

  - ```c
    // kernel/trap.c
    int sigalarm(int ticks, void(*handler)()) {
      struct proc *p = myproc();
      p->alarm_interval = ticks;
      p->alarm_handler = handler;
      p->alarm_ticks = ticks;
      return 0;
    }
    
    int sigreturn() {
      struct proc *p = myproc();
      *p->trapframe = *p->alarm_trapframe;
      p->alarm_goingoff = 0;
      return 0;
    }

- 修改`allocproc`和`freeproc`，实现初始化和释放

  - ```c
    // kernel/proc.c
    tatic struct proc*
    allocproc(void)
    {
    // ...
    found:
      p->pid = allocpid();
    
      // Allocate a trapframe page.
      if((p->trapframe = (struct trapframe *)kalloc()) == 0){
        release(&p->lock);
        return 0;
      }
    
      // Allocate a trapframe page for alarm_trapframe.
      if((p->alarm_trapframe = (struct trapframe *)kalloc()) == 0){
        release(&p->lock);
        return 0;
      }
    
      p->alarm_interval = 0;
      p->alarm_handler = 0;
      p->alarm_ticks = 0;
      p->alarm_goingoff = 0;
      // ...
    ```

  - ```c
    static void
    freeproc(struct proc *p)
    {
    // ...
      if(p->pagetable)
        proc_freepagetable(p->pagetable, p->sz);
      if(p->alarm_trapframe)
        kfree((void*)p->alarm_trapframe);
      p->alarm_trapframe = 0;
      p->alarm_interval = 0;
      p->alarm_handler = 0;
      p->alarm_ticks = 0;
      p->alarm_goingoff = 0;
      // ...
    ```

- 在`usertrap`中，实现时钟机制

  - 在每次时钟中断的时候，如果进程有已经设置的时钟（`alarm_interval != 0`），则进行 alarm_ticks 倒数。当 alarm_ticks 倒数到小于等于 0 的时候，如果没有正在处理的时钟，则尝试触发时钟，将原本的程序流保存起来（`*alarm_trapframe = *trapframe`），然后通过修改 pc 寄存器的值，将程序流转跳到 alarm_handler 中，alarm_handler 执行完毕后再恢复原本的执行流（`*trapframe = *alarm_trapframe`）。

  - ```c
    // kernel/trap.c
    void
    usertrap(void)
    {
        // ...
        
    // give up the CPU if this is a timer interrupt.
     if(which_dev == 2) {
        if(p->alarm_interval != 0) { 
          if(--p->alarm_ticks <= 0) { 
            if(!p->alarm_goingoff) { 
              p->alarm_ticks = p->alarm_interval;
              *p->alarm_trapframe = *p->trapframe; 
              p->trapframe->epc = (uint64)p->alarm_handler;
              p->alarm_goingoff = 1;
            }
          }
        }
        yield();
      }

## 结果

![image-20240511185526059](assets/image-20240511185526059.png)