

![线程池项目](readme.assets/线程池项目.png)





## 项目介绍

作为五大池之一（内存池、连接池、线程池、进程池、协程池），线程池的应用非常广泛，不管是客户端程序，还是后台服务程序，都是提高业务处理能力的必备模块。有很多开源的线程池实现，虽然各自接口使用上稍有区别，但是其核心实现原理都是基本相同的。

## 知识背景

- 熟练基于C++ 11标准的面向对象编程

  组合和继承、继承多态、STL容器、智能指针、函数对象、绑定器、可变参模板编程等。

- 熟悉C++11多线程编程

  thread、mutex、atomic、condition_variable、unique_lock等。

- C++17和C++20标准的内容

  C++17的any类型和C++20的信号量semaphore，项目上都我们自己用代码实现。

- 熟悉多线程理论
  多线程基本知识、线程互斥、线程同步、原子操作、CAS等。

## 项目代码片段

![image-20230305212829257](readme.assets/image-20230305212829257.png)

## 并发和并行

- CPU单核
- CPU多核、多CPU

### 并发

单核上，多个线程占用不同的CPU时间片，物理上还是串行执行的，但是由于每个线程占用的CPU时间片非常短（比如10ms），看起来就像是多个线程都在共同执行一样，这样的场景称作并发（concurrent）。

### 并行

在多核或者多CPU上，多个线程是在真正的同时执行，这样的场景称作并行（parallel）。

## 多线程的优势

多线程程序一定就好吗？不一定，要看具体的应用场景：

### IO密集型

无论是CPU单核、CPU多核、多CPU，都是比较适合多线程程序的

### CPU密集型

- CPU单核

多线程存在上下文切换，是额外的花销，线程越多上下文切换所花费的额外时间也越多，倒不如一个线程一直进行计算。

- CPU多核、多CPU

多个线程可以并行执行，对CPU利用率好。

## 线程池

### 线程的消耗

为了完成任务，创建很多的线程可以吗？线程真的是越多越好？

- 线程的创建和销毁都是非常"重"的操作
- 线程栈本身占用大量内存
- 线程的上下文切换要占用大量时间
- 大量线程同时唤醒会使系统经常出现锯齿状负载或者瞬间负载量很大导致宕机

### 线程池的优势

操作系统上创建线程和销毁线程都是很"重"的操作，耗时耗性能都比较多，那么在服务执行的过程中，如果业务量比较大，实时的去创建线程、执行业务、业务完成后销毁线程，那么会导致系统的实时性能降低，业务的处理能力也会降低。

线程池的优势就是（每个池都有自己的优势），在服务进程启动之初，就事先创建好线程池里面的线程，当业务流量到来时需要分配线程，直接从线程池中获取一个空闲线程执行task任务即可，task执行完成后，也不用释放线程，而是把线程归还到线程池中继续给后续的task提供服务。

#### fixed模式线程池

线程池里面的线程个数是固定不变的，一般是ThreadPool创建时根据当前机器的CPU核心数量进行指定。

#### cached模式线程池

线程池里面的线程个数是可动态增长的，根据任务的数量动态的增加线程的数量，但是会设置一个线程数量的阈值（线程过多的坏处上面已经讲过了），任务处理完成，如果动态增长的线程空闲了60s还没有处理其它任务，那么关闭线程，保持池中最初数量的线程即可。

## 线程同步

### 线程互斥

- 互斥锁mutex
- atomic原子类型

### 线程通信

- 条件变量 condition_variable
- 信号量 semaphore

## 项目设计

![image-20230305213003968](readme.assets/image-20230305213003968.png)

## 项目输出

### 应用到项目中

- 高并发网络服务器
- master-slave线程模型
- 耗时任务处理

### 输出到简历上

#### 项目名称：基于可变参模板实现的线程池

git地址：git@gitee.com:xxx/xxx.git
平台工具：vs2019开发，centos7 g++编译so库，gdb调试分析定位死锁问题

#### 项目描述：

1、基于可变参模板编程和引用折叠原理，实现线程池submitTask接口，支持任意任务函数和任意参数
的传递
2、使用future类型定制submitTask提交任务的返回值
3、使用map和queue容器管理线程对象和任务
4、基于条件变量condition_variable和互斥锁mutex实现任务提交线程和任务执行线程间的通信机制
5、支持fixed和cached模式的线程池定制
6、xxxx（自由发挥）

#### 项目问题：

1、在ThreadPool的资源回收，等待线程池所有线程退出时，发生死锁问题，导致进程无法退出
2、在windows平台下运行良好的线程池，在linux平台下运行发生死锁问题，平台运行结果有差异化

#### 分析定位问题：

主要通过gdb attach到正在运行的进程，通过info threads，thread tid，bt等命令查看各个线程的调用
堆栈信息，结合项目代码，定位到发生死锁的代码片段，分析死锁问题发生的原因，xxxx，以及最终的
解决方案。  

## linux平台编译线程池动态库

### 生成动态库

```cpp
g++ -fPIC -shared threadpool.cpp -o libtdpool.so -std=c++17
```

### 移动到lib目录

```cpp
mv libtdpool.so /usr/local/lib/
    
mv threadpool.h /usr/local/include/
```

### 编程测试程序

```cpp
g++ main.cpp -o main -std=c++17 -ltdpool -lpthread
```

此时运行生成的文件还是有错误的.

```
[root@localhost threadpool]# ./main 
./main: error while loading shared libraries: libtdpool.so: cannot open shared object file: No such file or directory
```

我们需要配置链接的路径

```
cd /etc/ld.so.conf.d/
vim mylib.conf
/usr/local/lib
```

刷新链接路径到 `/etc/ld.so.cache` 中

```
ldconfig
```





## linux下debug

### 查看当前用户下运行的进程

```
ps -u
```

```
[root@localhost ~]# ps -u
USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND
root       724  0.0  0.0 110204   860 tty1     Ss+  23:06   0:00 /sbin/agetty --noclear tty1 linux
root      1514  0.0  0.0 115676  2068 pts/0    Ss   23:07   0:00 -bash
root      1515  0.0  0.0 115676  2264 pts/1    Ss   23:07   0:00 -bash
root      1670  2.3  0.0 386676  1644 pts/1    Sl+  23:23   0:00 ./main
root      1677  0.0  0.0 155456  1876 pts/0    R+   23:23   0:00 ps -u
```

### 调试一个正在运行的进程

```
gdb attach pid
```

### 显示当前进程正在运行的线程

```
info threads
```

![image-20230307232531583](readme.assets/image-20230307232531583.png)

### 显示线程堆栈

```
bt
```

```
(gdb) bt
#0  0x00007fbab5156a35 in pthread_cond_wait@@GLIBC_2.3.2 () from /lib64/libpthread.so.0
#1  0x00007fbab4e3a5cc in __gthread_cond_wait (__mutex=<optimized out>, __cond=<optimized out>) at /home/sylar/soft/gcc-9.1.0/build/x86_64-pc-linux-gnu/libstdc++-v3/include/x86_64-pc-linux-gnu/bits/gthr-default.h:865
#2  std::condition_variable::wait (this=<optimized out>, __lock=...) at ../../../../../libstdc++-v3/src/c++11/condition_variable.cc:53
#3  0x00007fbab5385b80 in void std::condition_variable::wait<ThreadPool::~ThreadPool()::{lambda()#1}>(std::unique_lock<std::mutex>&, ThreadPool::~ThreadPool()::{lambda()#1}) () from /usr/local/lib/libtdpool.so
#4  0x00007fbab5384b6f in ThreadPool::~ThreadPool() () from /usr/local/lib/libtdpool.so
#5  0x0000000000402cf1 in main ()
```

### 切换线程

```
thread 2
```

```
(gdb) thread 2
[Switching to thread 2 (Thread 0x7fbab2c86700 (LWP 1674))]
#0  0x00007fbab515954d in __lll_lock_wait () from /lib64/libpthread.so.0
(gdb) bt
#0  0x00007fbab515954d in __lll_lock_wait () from /lib64/libpthread.so.0
#1  0x00007fbab5157240 in pthread_cond_broadcast@@GLIBC_2.3.2 () from /lib64/libpthread.so.0
#2  0x00007fbab4e3a609 in __gthread_cond_broadcast (__cond=<optimized out>) at /home/sylar/soft/gcc-9.1.0/build/x86_64-pc-linux-gnu/libstdc++-v3/include/x86_64-pc-linux-gnu/bits/gthr-default.h:853
#3  std::condition_variable::notify_all (this=<optimized out>) at ../../../../../libstdc++-v3/src/c++11/condition_variable.cc:73
#4  0x00007fbab5385b36 in Semaphore::post() () from /usr/local/lib/libtdpool.so
#5  0x00007fbab53859d0 in Result::setVal(Any) () from /usr/local/lib/libtdpool.so
#6  0x00007fbab5385877 in Task::exec() () from /usr/local/lib/libtdpool.so
#7  0x00007fbab538568f in ThreadPool::threadFunc(int) () from /usr/local/lib/libtdpool.so
......
```



### 原因

Result也是一个局部对象,出作用域后析构, 私有属性Semaphore也会析构, Semaphore的析构函数默认销毁私有属性 condition_variable , 在 windows 上condition_variable 析构默认会释放资源,调用它 notify_all() 什么都不做, 也不会阻塞. Linux下 condition_variable 析构后, 调用 notify_all 会阻塞. 

```cpp
int main()
{
	{
		ThreadPool pool;
		pool.setMode(ThreadPoolMode::MODE_CACHED);
		pool.start(4);

		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Result res4 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Result res5 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Result res6 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		 
		ULong sum1 = res1.get().cast_<ULong>();
		std::cout << sum1 << std::endl;
	}
}
```



#### MSVC 中的 condition_variable 析构函数

```cpp
~condition_variable() noexcept {
    _Cnd_destroy_in_situ(_Mycnd());
}
```

#### GCC中的 condition_variable 析构函数

```cpp
~condition_variable() noexcept;
```



```cpp
//信号量资源类 , 可以跨线程进行通信
class Semaphore
{
public:
	Semaphore(int limit = 0)
		: resLimit_(limit)
	{

	}
	~Semaphore() = default;

	// 获取一个信号量资源
	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		// 等待信号量有资源，没有资源的话，会阻塞当前线程
		cond_.wait(lock, [&]() -> bool { return resLimit_ > 0; });
		resLimit_--;
	}
	// 增加一个信号量资源
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		// linux下condition_variable的析构函数什么也不做
		// 导致这里状态已经失效,无故阻塞
		cond_.notify_all();
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};
```



