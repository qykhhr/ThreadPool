#include "threadpool.h"
#include<iostream>

const int TASK_MAX_THREADHOLD = 4;

// 线程池构造
ThreadPool::ThreadPool()
	: initThreadSize_(4)
	, taskSize_(0)
	, taskQueMaxThreadsHold_(TASK_MAX_THREADHOLD)
	, poolMode_(ThreadPoolMode::MODE_FIXED)
{

}
// 线程池析构
ThreadPool::~ThreadPool()
{

}
// 设置线程池的工作模式
void ThreadPool::setMode(ThreadPoolMode mode)
{
	poolMode_ = mode;
}
// 设置task任务队列上限阈值
void ThreadPool::setTaskQueMaxThreadHold(int threadHold)
{
	taskQueMaxThreadsHold_ = threadHold;
}

// 给线程池提交任务 用户调用该接口，传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	// 获取锁 
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// 线程的通信 等待任务队列有空余
	/*while (_taskQue.size() == _taskQueMaxThreadsHold)
	{
		_notFull.wait(lock);
	}*/
	//_notFull.wait(lock, [&]() -> bool { return _taskQue.size() < _taskQueMaxThreadsHold; });
	// 用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool { return taskQue_.size() <    taskQueMaxThreadsHold_; }))
	{
		// 表示 _notFull 等待1s钟，条件依旧没有满足
		std::cerr << "task queue is full, submit task fail." << std::endl;
		// return task->getResult(); // 线程执行完task，task对象就被析构掉了
		return Result(sp,false);
	}

	// 如果有空余，把任务放入任务队列中
	taskQue_.emplace(sp);
	taskSize_++;
	// 因为新放了任务，任务队列肯定不为空了，在_notEmpty上进行通知
	notEmpty_.notify_all();
	return Result(sp);
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
	// 记录初始线程个数
	initThreadSize_ = initThreadSize;
	// 创建线程对象
	for (int i = 0; i < initThreadSize_; ++i)
	{
		// 创建thread线程对象的适合，把线程函数给到thread线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this)); // bind绑定器需要绑定成员方法和成员指针(默认参数)
		// unique_ptr 不允许拷贝赋值，只能进行右值引用资源转移
		threads_.emplace_back(std::move(ptr));
		//_threads.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));
	}
	// 启动所有线程  std::vector<Thread*> _threads;
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threads_[i]->start(); //指定线程函数
	}

}

// 定义线程函数  线程池的所有线程从任务队列里面消费任务 
void ThreadPool::threadFunc()
{
	/*std::cout << "begin threadFunc tid:" << std::this_thread::get_id() << std::endl;
	std::cout << "end threadFunc tid:" << std::this_thread::get_id() << std::endl;*/
	for (;;)
	{
		std::shared_ptr<Task> task;
		// 保证任务队列获取安全, 获取任务后就可以把锁释放掉
		{
			// 先获取锁, 出作用域锁就会被释放掉
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id()
				<< "尝试获取任务..." << std::endl;

			// 等待notEmpty条件
			notEmpty_.wait(lock, [&]() -> bool { return taskQue_.size() > 0; });
			
			std::cout << "tid:" << std::this_thread::get_id()
				<< "获取任务成功..." << std::endl;
			
			// 从任务队列中取出一个任务出来
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			// 取出一个任务后，队列就不是满的了，可以进行通知
			notFull_.notify_all();
			// 如果依然有剩余任务，继续通知其他线程执行任务
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}
		} // 获取任务后就可以把锁释放掉

		// 当前线程负责执行这个任务
		if (task != nullptr)
		{
			//task->run(); 
			//执行任务; 把任务的返回值setVal方法给到Result
			task->exec();
		}
	}
}

///////////////////// 线程方法实现 /////////////////////
// 线程构造
Thread::Thread(ThreadFunc func)
	: _func(func)
{
}
// 线程析构
Thread::~Thread()
{

}
// 启动线程
void Thread::start()
{
	// 创建一个线程来执行一个线程函数
	std::thread t(_func); // C++11来说 线程对象t 和 线程函数_func
	t.detach(); // 设置分离线程  pthread_detach pthread_t 
}

///////////////////// Task方法实现 /////////////////////
Task::Task()
	: result_(nullptr)
{

}
void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run()); //这里发生多态调用
	}
}
void Task::setResult(Result* res)
{
	result_ = res;
}

///////////////////// Result方法实现 /////////////////////
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: task_(task)
	, isValid_(isValid)
{
	task_->setResult(this);
}
// 问题一: setVal方法，获取任务执行完的返回值的
void Result::setVal(Any res)
{
	// 存储task的返回值
	this->any_ = std::move(res);
	sem_.post(); //已经获取的任务的返回值,增加信号量资源
}
// 问题二: get 方法，用户调用这个方法获取task的返回值
Any Result::get() // 用户调用的
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait(); // task任务如果没有执行完,这里会阻塞用户线程
	return std::move(any_);
}

///////////////////// Semaphore方法实现 /////////////////////

Semaphore::Semaphore(int limit)
	: resLimit_(limit)
{

}
// 获取一个信号量资源
void Semaphore::wait()
{
	std::unique_lock<std::mutex> lock(mtx_);
	// 等待信号量有资源，没有资源的话，会阻塞当前线程
	cond_.wait(lock, [&]() -> bool { return resLimit_ > 0; });
	resLimit_--;
}
// 增加一个信号量资源
void Semaphore::post()
{
	std::unique_lock<std::mutex> lock(mtx_);
	resLimit_++;
	cond_.notify_all();
}


