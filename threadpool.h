#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<iostream>
#include<functional>

//信号量资源类
class Semaphore
{
public:
	Semaphore(int limit = 0)
		: _resLimit(limit)
	{

	}
	~Semaphore() = default;

	// 获取一个信号量资源
	void wait()
	{
		std::unique_lock<std::mutex> lock(_mtx);
		// 等待信号量有资源，没有资源的话，会阻塞当前线程
		_cond.wait(lock, [&]() -> bool { return _resLimit > 0; });
		_resLimit--;
	}
	// 增加一个信号量资源
	void post()
	{
		std::unique_lock<std::mutex> lock(_mtx);
		_resLimit++;
		_cond.notify_all();
	}
private:
	int _resLimit;
	std::mutex _mtx;
	std::condition_variable _cond;
};


//// Any类型: 可以接受任意数据的类型
//class Any
//{
//public:
//	Any() = default;
//	~Any() = default;
//
//	// 这个构造函数可以让Any类型接受任意其他的数据
//	template<typename T>
//	Any(T data) : _base(std::make_unique<Derive<T>>(data)))
//	{
//		
//	}
//	//这个方法能把Any对象里面存储的data数据提取出来
//	template<typename T>
//	T cast()
//	{
//		// 我们怎么从_base找到它所指向的Derive对象，从它里面取出data成员变量
//		// 基类指针 ==> 派生类指针 RTTI
//		Derive<T>* pd = dynamic_cast<Derive<T>*>(_base.get());
//		if (pd == nullptr)
//		{
//			throw "type is notmatch!";
//		}
//		return pd->_data;
//	}
//private:
//	// 基类类型
//	class Base
//	{
//	public:
//		~Base() = default;
//	};
//	// 派生类类型
//	template<typename T>
//	class Derive : public Base
//	{
//	public:
//		Derive(T data) : _data(data)
//		{
//		}
//		T _data;
//	};
//private:
//	// 定义一个基类指针
//	std::unique_ptr<Base> _base;
//};


// Task类型的前置声明
class Task;

//// 实现接受提交到线程池的task任务执行完成后的返回值类型Result
//class Result
//{
//public:
//	Result(std::shared_ptr<Task> task, bool isValid = true);
//	~Result() = default;
//	// 问题一: setVal方法，获取任务执行完的返回值的
//	void setVal(Result res);
//	// 问题二: get 方法，用户调用这个方法获取task的返回值
//	Any get();
//private:
//	Any _any; /// 存储任务的返回值
//	Semaphore _sem; // 线程通信信号量
//	std::shared_ptr<Task> _task; // 执行对应获取返回值的任务对象
//	std::atomic_bool _isValid; // 返回值是否有效
//};

// 任务抽象基类
class Task
{
public:
	// 用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
	virtual void run() = 0;
	void exec();
	//void setResult(Result* res);
private:
	//Result* _result;
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
	using ThreadFunc = std::function<void()>;
	
	// 线程构造
	Thread(ThreadFunc func);
	// 线程析构
	~Thread();
		
	// 启动线程
	void start();

private:
	ThreadFunc _func;
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
	

	// 给线程池提交任务
	void submitTask(std::shared_ptr<Task> sp);

	// 开启线程池
	void start(int initThreadSize = 4);

	// 禁止对象赋值、拷贝操作
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// 定义线程函数
	void threadFunc();

private:
	std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
	size_t initThreadSize_; // 初始的线程数量
	
	// 用户可能传入生命周期短的对象(临时对象的指针)
	std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
	std::atomic_int taskSize_; // 任务数量
	size_t taskQueMaxThreadsHold_; // 任务队列数量上限阈值, 既定上限

	std::mutex taskQueMtx_; // 保证任务队列的线程安全
	std::condition_variable notFull_; // 表示任务队列不满
	std::condition_variable notEmpty_; // 表示任务队列不空

	ThreadPoolMode poolMode_; // 当前线程池的工作模式
};

#endif