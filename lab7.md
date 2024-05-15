# Task1

- 您的工作是提出一个创建线程和保存/恢复寄存器以在线程之间切换的计划，并实现该计划。完成后，`make grade`应该表明您的解决方案通过了`uthread`测试。



## 提示

- `thread_switch`只需要保存/还原被调用方保存的寄存器（callee-save register，参见LEC5使用的文档《Calling Convention》）。为什么？
- 您可以在***user/uthread.asm\***中看到`uthread`的汇编代码，这对于调试可能很方便。

## 实现

- `uthread_switch.S`的实现和内核中的上下文切换`swtch.S`完全一致

  - 在调用过程中，调用者应该保存的寄存器已经被其放入栈帧中了，所以无需保存

  - ```
    	.text
    
    	/*
             * save the old thread's registers,
             * restore the new thread's registers.
             */
    
    	.globl thread_switch
    thread_switch:
    	/* YOUR CODE HERE */
    	sd ra, 0(a0)
    	sd sp, 8(a0)
    	sd s0, 16(a0)
    	sd s1, 24(a0)
    	sd s2, 32(a0)
    	sd s3, 40(a0)
    	sd s4, 48(a0)
    	sd s5, 56(a0)
    	sd s6, 64(a0)
    	sd s7, 72(a0)
    	sd s8, 80(a0)
    	sd s9, 88(a0)
    	sd s10, 96(a0)
    	sd s11, 104(a0)
    
    	ld ra, 0(a1)
    	ld sp, 8(a1)
    	ld s0, 16(a1)
    	ld s1, 24(a1)
    	ld s2, 32(a1)
    	ld s3, 40(a1)
    	ld s4, 48(a1)
    	ld s5, 56(a1)
    	ld s6, 64(a1)
    	ld s7, 72(a1)
    	ld s8, 80(a1)
    	ld s9, 88(a1)
    	ld s10, 96(a1)
    	ld s11, 104(a1)
    	ret    /* return to ra */
    ```

- 为 `thread` 结构体增加一项 `context` 用于保存上下文，`context`也是一个结构体

  - ```c
    // kernel/uthread.c
    struct context {
      uint64 ra;
      uint64 sp;
    
      // callee-saved
      uint64 s0;
      uint64 s1;
      uint64 s2;
      uint64 s3;
      uint64 s4;
      uint64 s5;
      uint64 s6;
      uint64 s7;
      uint64 s8;
      uint64 s9;
      uint64 s10;
      uint64 s11;
    };
    
    struct thread {
      char       stack[STACK_SIZE]; /* the thread's stack */
      int        state;             /* FREE, RUNNING, RUNNABLE */
      struct context ctx;
    };
    ```

- 修改 `thread_schedule`，实现上下文切换

  - ```c
    // kernel/uthread.c
    if (current_thread != next_thread) {         /* switch threads?  */
        next_thread->state = RUNNING;
        t = current_thread;
        current_thread = next_thread;
        /* YOUR CODE HERE
         * Invoke thread_switch to switch from t to next_thread:
         * thread_switch(??, ??);
         */
        thread_switch(&t->ctx, &next_thread->ctx);
      } else
        next_thread = 0;
    ```

- 修改 `thread_create`，线程创建时需要设置上下文中的ra和sp指针

  - ```c
    void 
    thread_create(void (*func)())
    {
      struct thread *t;
    
      for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
        if (t->state == FREE) break;
      }
      t->state = RUNNABLE;
      // YOUR CODE HERE
      t->state = RUNNABLE;
      t->ctx.ra = (uint64)func;      
      t->ctx.sp = (uint64)&t->stack + (STACK_SIZE - 1); 
    }
    ```

## 结果

![image-20240515193637242](assets/image-20240515193637242.png)



# Task2

为什么两个线程都丢失了键，而不是一个线程？确定可能导致键丢失的具有2个线程的事件序列。在***answers-thread.txt\***中提交您的序列和简短解释。

[!TIP] 为了避免这种事件序列，请在***notxv6/ph.c\***中的`put`和`get`中插入`lock`和`unlock`语句，以便在两个线程中丢失的键数始终为0。相关的pthread调用包括：

- `pthread_mutex_t lock; // declare a lock`
- `pthread_mutex_init(&lock, NULL); // initialize the lock`
- `pthread_mutex_lock(&lock); // acquire lock`
- `pthread_mutex_unlock(&lock); // release lock`

当`make grade`说您的代码通过`ph_safe`测试时，您就完成了，该测试需要两个线程的键缺失数为0。在此时，`ph_fast`测试失败是正常的。

修改代码，使某些`put`操作在保持正确性的同时并行运行。当`make grade`说你的代码通过了`ph_safe`和`ph_fast`测试时，你就完成了。`ph_fast`测试要求两个线程每秒产生的`put`数至少是一个线程的1.25倍。

## 提示

- 不要忘记调用`pthread_mutex_init()`。首先用1个线程测试代码，然后用2个线程测试代码。您主要需要测试：程序运行是否正确呢（即，您是否消除了丢失的键？）？与单线程版本相比，双线程版本是否实现了并行加速（即单位时间内的工作量更多）？
- 在某些情况下，并发`put()`在哈希表中读取或写入的内存中没有重叠，因此不需要锁来相互保护。您能否更改***ph.c***以利用这种情况为某些`put()`获得并行加速？提示：每个散列桶加一个锁怎么样？

## 实现

- 回答问题

- ```
  [假设键 k1、k2 属于同个 bucket]
  
  thread 1: 尝试设置 k1
  thread 1: 发现 k1 不存在，尝试在 bucket 末尾插入 k1
  --- scheduler 切换到 thread 2
  thread 2: 尝试设置 k2
  thread 2: 发现 k2 不存在，尝试在 bucket 末尾插入 k2
  thread 2: 分配 entry，在桶末尾插入 k2
  --- scheduler 切换回 thread 1
  thread 1: 分配 entry，没有意识到 k2 的存在，在其认为的 “桶末尾”（实际为 k2 所处位置）插入 k1
  
  [k1 被插入，但是由于被 k1 覆盖，k2 从桶中消失了，引发了键值丢失]

- 完成安全性，只需要给`put`和`get`加锁即可

  - ```c
    // ph.c
    pthread_mutex_t lock;
    
    int
    main(int argc, char *argv[])
    {
      pthread_t *tha;
      void *value;
      double t1, t0;
      
      pthread_mutex_init(&lock, NULL);
    
      // ......
    }
    
    static 
    void put(int key, int value)
    {
       NBUCKET;
    
      pthread_mutex_lock(&lock);
      
      // ......
    
      pthread_mutex_unlock(&lock);
    }
    
    static struct entry*
    get(int key)
    {
      $ NBUCKET;
    
      pthread_mutex_lock(&lock);
      
      // ......
    
      pthread_mutex_unlock(&lock);
    
      return e;
    }
    
    ```

- 但为所有操作加锁意味着每一时刻只能有一个线程在操作哈希表，这里实际上等同于将哈希表的操作变回单线程了，又由于锁操作（加锁、解锁、锁竞争）是有开销的，所以性能甚至不如单线程版本。

- 考虑到哈希表的特性，只需要对每个桶加锁即可

  - ```c
    // ph.c
    pthread_mutex_t locks[NBUCKET];
    
    int
    main(int argc, char *argv[])
    {
      pthread_t *tha;
      void *value;
      double t1, t0;
      
      for(int i=0;i<NBUCKET;i++) {
        pthread_mutex_init(&locks[i], NULL); 
      }
    
      // ......
    }
    
    static 
    void put(int key, int value)
    {
      int i = key % NBUCKET;
    
      pthread_mutex_lock(&locks[i]);
      
      // ......
    
      pthread_mutex_unlock(&locks[i]);
    }
    
    static struct entry*
    get(int key)
    {
      int i = key % NBUCKET;
    
      pthread_mutex_lock(&locks[i]);
      
      // ......
    
      pthread_mutex_unlock(&locks[i]);
    
      return e;
    }
    

## 结果

![image-20240515195736492](assets/image-20240515195736492.png)



# Task3

在本作业中，您将实现一个[屏障](http://en.wikipedia.org/wiki/Barrier_(computer_science))（Barrier）：应用程序中的一个点，所有参与的线程在此点上必须等待，直到所有其他参与线程也达到该点。您将使用pthread条件变量，这是一种序列协调技术，类似于xv6的`sleep`和`wakeup`。

## 提示

`pthread_cond_wait`在调用时释放`mutex`，并在返回前重新获取`mutex`。

我们已经为您提供了`barrier_init()`。您的工作是实现`barrier()`，这样panic就不会发生。我们为您定义了`struct barrier`；它的字段供您使用。

**有两个问题使您的任务变得复杂：**

- 你必须处理一系列的`barrier`调用，我们称每一连串的调用为一轮（round）。`bstate.round`记录当前轮数。每次当所有线程都到达屏障时，都应增加`bstate.round`。
- 您必须处理这样的情况：一个线程在其他线程退出`barrier`之前进入了下一轮循环。特别是，您在前后两轮中重复使用`bstate.nthread`变量。确保在前一轮仍在使用`bstate.nthread`时，离开`barrier`并循环运行的线程不会增加`bstate.nthread`。

使用一个、两个和两个以上的线程测试代码。

## 实现

- 线程进入同步屏障 barrier 时，将已进入屏障的线程数量增加 1，然后再判断是否已经达到总线程数。
  如果未达到，则进入睡眠，等待其他线程。
  如果已经达到，则唤醒所有在 barrier 中等待的线程，所有线程继续执行；屏障轮数 + 1；

  - ```c
    static void 
    barrier()
    {
      // YOUR CODE HERE
      //
      // Block until all threads have called barrier() and
      // then increment bstate.round.
      //
      pthread_mutex_lock(&bstate.barrier_mutex);
      if(++bstate.nthread < nthread) {
        pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
      } else {
        bstate.nthread = 0;
        bstate.round++;
        pthread_cond_broadcast(&bstate.barrier_cond);
      }
      pthread_mutex_unlock(&bstate.barrier_mutex);
    }
    ```

## 结果

![image-20240515200654544](assets/image-20240515200654544.png)