#include "threadpool.h"
#include<iostream>

const int TASK_MAX_THREADHOLD = 4;

// �̳߳ع���
ThreadPool::ThreadPool()
	: initThreadSize_(4)
	, taskSize_(0)
	, taskQueMaxThreadsHold_(TASK_MAX_THREADHOLD)
	, poolMode_(ThreadPoolMode::MODE_FIXED)
{

}
// �̳߳�����
ThreadPool::~ThreadPool()
{

}
// �����̳߳صĹ���ģʽ
void ThreadPool::setMode(ThreadPoolMode mode)
{
	poolMode_ = mode;
}
// ����task�������������ֵ
void ThreadPool::setTaskQueMaxThreadHold(int threadHold)
{
	taskQueMaxThreadsHold_ = threadHold;
}

// ���̳߳��ύ���� �û����øýӿڣ��������������������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	// ��ȡ�� 
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// �̵߳�ͨ�� �ȴ���������п���
	/*while (_taskQue.size() == _taskQueMaxThreadsHold)
	{
		_notFull.wait(lock);
	}*/
	//_notFull.wait(lock, [&]() -> bool { return _taskQue.size() < _taskQueMaxThreadsHold; });
	// �û��ύ�����������������1s�������ж��ύ����ʧ�ܣ�����
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool { return taskQue_.size() <    taskQueMaxThreadsHold_; }))
	{
		// ��ʾ _notFull �ȴ�1s�ӣ���������û������
		std::cerr << "task queue is full, submit task fail." << std::endl;
		// return task->getResult(); // �߳�ִ����task��task����ͱ���������
		return Result(sp,false);
	}

	// ����п��࣬������������������
	taskQue_.emplace(sp);
	taskSize_++;
	// ��Ϊ�·�������������п϶���Ϊ���ˣ���_notEmpty�Ͻ���֪ͨ
	notEmpty_.notify_all();
	return Result(sp);
}

// �����̳߳�
void ThreadPool::start(int initThreadSize)
{
	// ��¼��ʼ�̸߳���
	initThreadSize_ = initThreadSize;
	// �����̶߳���
	for (int i = 0; i < initThreadSize_; ++i)
	{
		// ����thread�̶߳�����ʺϣ����̺߳�������thread�̶߳���
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this)); // bind������Ҫ�󶨳�Ա�����ͳ�Աָ��(Ĭ�ϲ���)
		// unique_ptr ����������ֵ��ֻ�ܽ�����ֵ������Դת��
		threads_.emplace_back(std::move(ptr));
		//_threads.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));
	}
	// ���������߳�  std::vector<Thread*> _threads;
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threads_[i]->start(); //ָ���̺߳���
	}

}

// �����̺߳���  �̳߳ص������̴߳�������������������� 
void ThreadPool::threadFunc()
{
	/*std::cout << "begin threadFunc tid:" << std::this_thread::get_id() << std::endl;
	std::cout << "end threadFunc tid:" << std::this_thread::get_id() << std::endl;*/
	for (;;)
	{
		std::shared_ptr<Task> task;
		// ��֤������л�ȡ��ȫ, ��ȡ�����Ϳ��԰����ͷŵ�
		{
			// �Ȼ�ȡ��, �����������ͻᱻ�ͷŵ�
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id()
				<< "���Ի�ȡ����..." << std::endl;

			// �ȴ�notEmpty����
			notEmpty_.wait(lock, [&]() -> bool { return taskQue_.size() > 0; });
			
			std::cout << "tid:" << std::this_thread::get_id()
				<< "��ȡ����ɹ�..." << std::endl;
			
			// �����������ȡ��һ���������
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			// ȡ��һ������󣬶��оͲ��������ˣ����Խ���֪ͨ
			notFull_.notify_all();
			// �����Ȼ��ʣ�����񣬼���֪ͨ�����߳�ִ������
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}
		} // ��ȡ�����Ϳ��԰����ͷŵ�

		// ��ǰ�̸߳���ִ���������
		if (task != nullptr)
		{
			//task->run(); 
			//ִ������; ������ķ���ֵsetVal��������Result
			task->exec();
		}
	}
}

///////////////////// �̷߳���ʵ�� /////////////////////
// �̹߳���
Thread::Thread(ThreadFunc func)
	: _func(func)
{
}
// �߳�����
Thread::~Thread()
{

}
// �����߳�
void Thread::start()
{
	// ����һ���߳���ִ��һ���̺߳���
	std::thread t(_func); // C++11��˵ �̶߳���t �� �̺߳���_func
	t.detach(); // ���÷����߳�  pthread_detach pthread_t 
}

///////////////////// Task����ʵ�� /////////////////////
Task::Task()
	: result_(nullptr)
{

}
void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run()); //���﷢����̬����
	}
}
void Task::setResult(Result* res)
{
	result_ = res;
}

///////////////////// Result����ʵ�� /////////////////////
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: task_(task)
	, isValid_(isValid)
{
	task_->setResult(this);
}
// ����һ: setVal��������ȡ����ִ����ķ���ֵ��
void Result::setVal(Any res)
{
	// �洢task�ķ���ֵ
	this->any_ = std::move(res);
	sem_.post(); //�Ѿ���ȡ������ķ���ֵ,�����ź�����Դ
}
// �����: get �������û��������������ȡtask�ķ���ֵ
Any Result::get() // �û����õ�
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait(); // task�������û��ִ����,����������û��߳�
	return std::move(any_);
}

///////////////////// Semaphore����ʵ�� /////////////////////

Semaphore::Semaphore(int limit)
	: resLimit_(limit)
{

}
// ��ȡһ���ź�����Դ
void Semaphore::wait()
{
	std::unique_lock<std::mutex> lock(mtx_);
	// �ȴ��ź�������Դ��û����Դ�Ļ�����������ǰ�߳�
	cond_.wait(lock, [&]() -> bool { return resLimit_ > 0; });
	resLimit_--;
}
// ����һ���ź�����Դ
void Semaphore::post()
{
	std::unique_lock<std::mutex> lock(mtx_);
	resLimit_++;
	cond_.notify_all();
}


