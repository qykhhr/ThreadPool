#include "threadpool.h"
#include<iostream>

const int TASK_MAX_THREADHOLD = INT32_MAX;//队列上限阈值
const int THREAD_MAX_THRESHOLD = 1024;//线程上限阈值
const int THREAD_MAX_IDLE_TIME = 60; //线程空闲时间,单位:秒

// 线程池构造
ThreadPool::ThreadPool()
	: initThreadSize_(4)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THREADHOLD)
	, poolMode_(ThreadPoolMode::MODE_FIXED)
	, isPoolRunning_(false)
	, idleThreadSize_(0)
	, threadSizeThreshHold_(THREAD_MAX_THRESHOLD)
	, curThreadSize_(0)
{

}
// 线程池析构
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	// 等待线程池里面所有的线程返回 有两种状态: 阻塞 & 正在执行任务中
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, 
		[&]() -> bool { return threads_.size() == 0; });
}
// 设置线程池的工作模式
void ThreadPool::setMode(ThreadPoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}
// 设置task任务队列上限阈值
void ThreadPool::setTaskQueMaxThreadHold(int threadHold)
{
	if (checkRunningState())
		return;
	taskQueMaxThreshHold_ = threadHold;
}
void ThreadPool::setThreadSizeThresHold(int threadHold)
{
	if (checkRunningState())
		return;
	if(poolMode_ == ThreadPoolMode::MODE_CACHED)
		threadSizeThreshHold_ = threadHold;
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
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool { return taskQue_.size() <    taskQueMaxThreshHold_; }))
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

	// cached模式,任务处理比较紧急 场景: 小而快的任务,需要根据任务数量和空闲线程的数量,判断是否需要扩容线程
	if (poolMode_ == ThreadPoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreshHold_)
	{
		std::cout << "crate new thread... threaid:" << std::this_thread::get_id()
			<< " exit!" << std::endl;

		//创建新线程
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)); // bind绑定器需要绑定成员方法和成员指针(默认参数)
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//启动线程 
		threads_[threadId]->start(); 
		//修改线程个数相关的变量
		curThreadSize_++; // 虽然 threads_.size() 可以代表线程总数,但是vector不是线程安全的
		idleThreadSize_++; //空闲线程数量
	}

	return Result(sp);
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
	//设置线程池的运行状态
	isPoolRunning_ = true;

	// 记录初始线程个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;
	// 创建线程对象
	for (int i = 0; i < initThreadSize_; ++i)
	{
		// 创建thread线程对象的适合，把线程函数给到thread线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)); // bind绑定器需要绑定成员方法和成员指针(默认参数)
		// unique_ptr 不允许拷贝赋值，只能进行右值引用资源转移
		//threads_.emplace_back(std::move(ptr));
		int threadId = ptr->getId();
		threads_.emplace(threadId,std::move(ptr));
		//_threads.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));
	}
	// 启动所有线程  std::vector<Thread*> _threads;
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threads_[i]->start(); //指定线程函数
		idleThreadSize_++; //记录初始空闲线程的数量
	}

}

// 定义线程函数  线程池的所有线程从任务队列里面消费任务 
void ThreadPool::threadFunc(int threadId)
{
	// 记录每个线程的起始时间
	auto lastTime = std::chrono::high_resolution_clock().now();
	while(1)
	{
		std::shared_ptr<Task> task;
		// 保证任务队列获取安全, 获取任务后就可以把锁释放掉
		{
			// 先获取锁, 出作用域锁就会被释放掉
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id()
				<< "尝试获取任务..." << std::endl;

			// cached模式下,有可能已经创建了很多的线程,但是空闲时间超过60s,应该把多余的线程结束回收掉(超过initThreadSize_数量的线程要进行回收)
			//当前时间 - 上次线程执行的时间 > 60s
			//每秒钟返回一次, 怎么区分: 超时返回? 还是有任务待执行返回
				//如果任务队列数量为0开始准备回收线程资源
			while (taskQue_.size() == 0)
			{
				// 线程池要结束,回收线程资源
				if (!isPoolRunning_)
				{
					threads_.erase(threadId);
					std::cout << "threadid:" << std::this_thread::get_id()
						<< " exit" << std::endl;
					exitCond_.notify_all(); //线程函数结束,线程结束
					return;
				}
				
				if (poolMode_ == ThreadPoolMode::MODE_CACHED)
				{
					// 条件变量超时返回了
					if (std::cv_status::timeout ==
						notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_)
						{
							//开始回收当前线程
							//记录线程数量的相关变量的值修改
							//把线程对象从线程列表容器中删除, 没有办法 threadFun <==> thread对象 ,需要在thread对象中添加属性
							// threadId ==> thread对象 ==> 删除
							threads_.erase(threadId);
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "threadid:" << std::this_thread::get_id()
								<< " exit" << std::endl;

							return;
						}
					}
				}
				else
				{
					// 等待notEmpty条件
					notEmpty_.wait(lock);
				}

				//// 线程池要结束,回收线程资源
				//if (!isPoolRunning_)
				//{
				//	threads_.erase(threadId);
				//	std::cout << "threadid:" << std::this_thread::get_id()
				//		<< " exit" << std::endl;
				//	exitCond_.notify_all();
				//	return;
				//}
			}
			
			idleThreadSize_--; //记录初始空闲线程的数量

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
		idleThreadSize_++; //记录初始空闲线程的数量
		lastTime = std::chrono::high_resolution_clock().now(); // 更新起始时间
	}
}
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}

///////////////////// 线程方法实现 /////////////////////
int Thread::generateId_ = 0;
// 线程构造
Thread::Thread(ThreadFunc func)
	: func_(func)
	, threadId_(generateId_++)
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
	std::thread t(func_,threadId_); // C++11来说 线程对象t 和 线程函数_func
	t.detach(); // 设置分离线程  pthread_detach pthread_t 
}
int Thread::getId() const
{
	return threadId_;
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
		result_->setVal( run() ); //这里发生多态调用
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

