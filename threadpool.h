#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include<thread>
#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<iostream>
#include<functional>
//信号量资源类 , 可以跨线程进行通信
class Semaphore
{
public:
	Semaphore(int limit = 0)
		: resLimit_(limit)
		, isExit_(false)
	{

	}
	~Semaphore()
	{
		isExit_ = true;
	}

	// 获取一个信号量资源
	void wait()
	{
		if (isExit_)
			return;
		std::unique_lock<std::mutex> lock(mtx_);
		// 等待信号量有资源，没有资源的话，会阻塞当前线程
		cond_.wait(lock, [&]() -> bool { return resLimit_ > 0; });
		resLimit_--;
	}
	// 增加一个信号量资源
	void post()
	{
		if (isExit_)
			return;
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		// linux下condition_variable的析构函数什么也不做
		// 导致这里状态已经失效,无故阻塞
		cond_.notify_all();
	}
private:
	std::atomic_bool isExit_;
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};


// Any类型: 可以接受任意数据的类型
class Any
{
public:
	Any() = default;
	~Any() = default;

	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;

	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	// 这个构造函数可以让Any类型接受任意其他的数据
	template<typename T>
	Any(T data)
		: base_(std::make_unique<Derive<T>>(data))
	{

	}

	// 这个方法能把Any对象里面存储的data数据提取出来
	template<typename T>
	T cast_()
	{
		// 我们怎么从base_找到它所指向的Derive对象,从它里面取出data成员变量
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type id unmatch!";
		}
		return pd->data_;
	}
private:
	// 基类类型
	class Base
	{
	public:
		virtual ~Base() = default;
	};
	// 派生类类型
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data) : data_(data)
		{

		}
	public:
		T data_;
	};
private:
	// 定义一个基类指针 
	std::unique_ptr<Base> base_;
};

class Task;

// 实现接受提交到线程池的task任务执行完成后的返回值类型Result
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;
	// 问题一: setVal方法，获取任务执行完的返回值的
	void setVal(Any res);
	// 问题二: get 方法，用户调用这个方法获取task的返回值
	Any get(); // 用户调用的
private:
	Any any_; /// 存储任务的返回值
	Semaphore sem_; // 线程通信信号量
	std::shared_ptr<Task> task_; // 执行对应获取返回值的任务对象
	std::atomic_bool isValid_; // 返回值是否有效
};

// 任务抽象基类
class Task
{
public:
	Task();
	~Task() = default;

	// 用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
	virtual Any run() = 0;
	void exec();
	void setResult(Result* res);
private:
	Result* result_;
};


// 线程池支持的模式
enum class ThreadPoolMode // 加上 class 可以使用 ThreadPoolMode::MODE_FIXED 来选用，避免多个枚举类型名称相同引发冲突
{
	MODE_FIXED, // 固定数量的线程池
	MODE_CACHED // 线程数量可动态增长
};
// 线程类型
class Thread
{
public:
	// 线程函数对象模型
	using ThreadFunc = std::function<void(int)>;
	
	// 线程构造
	Thread(ThreadFunc func);
	// 线程析构
	~Thread();
		
	// 启动线程
	void start();

	int getId() const;
private:
	static int generateId_;
private:
	ThreadFunc func_;
	int threadId_;//保存线程id,为了从线程池中删除线程
};
// 线程池
class ThreadPool
{
public:
	// 线程池构造
	ThreadPool();
	// 线程池析构
	~ThreadPool();

	// 设置线程池的工作模式
	void setMode(ThreadPoolMode mode);

	// 设置task任务队列上限阈值
	void setTaskQueMaxThreadHold(int threadHold);
	
	// 设置线程池Cached模式下线程阈值
	void setThreadSizeThresHold(int threadHold);

	// 给线程池提交任务
	Result submitTask(std::shared_ptr<Task> sp);

	// 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency()); // 返回CPU的核心数量:std::thread::hardware_concurrency()

	// 禁止对象赋值、拷贝操作
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// 定义线程函数
	void threadFunc(int threadId);

	// 检查pool的运行状态
	bool checkRunningState() const;
private:
	//std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_; //线程列表
	size_t initThreadSize_; // 初始的线程数量
	
	// 用户可能传入生命周期短的对象(临时对象的指针)
	std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
	std::atomic_int taskSize_; // 任务数量
	size_t taskQueMaxThreshHold_; // 任务队列数量上限阈值, 既定上限

	std::mutex taskQueMtx_; // 保证任务队列的线程安全
	std::condition_variable notFull_; // 表示任务队列不满
	std::condition_variable notEmpty_; // 表示任务队列不空
	std::condition_variable exitCond_; //等待线程资源全部回收

	ThreadPoolMode poolMode_; // 当前线程池的工作模式

	// CACHE模式的线程池
	std::atomic_bool isPoolRunning_; // 表示当前线程池的启动状态
	std::atomic_int idleThreadSize_; //空闲线程数量
	size_t threadSizeThreshHold_; // 线程数量上限阈值
	std::atomic_int curThreadSize_; //记录当前线程池里面线程的总数量
};

#endif