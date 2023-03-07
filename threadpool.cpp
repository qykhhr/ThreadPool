#include "threadpool.h"
#include<iostream>

const int TASK_MAX_THREADHOLD = INT32_MAX;
const int THREAD_MAX_THRESHOLD = 1024;//�̴߳�����ֵ
const int THREAD_MAX_IDLE_TIME = 60; //�߳̿���ʱ��,��λ:��

// �̳߳ع���
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
// �̳߳�����
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	// �ȴ��̳߳��������е��̷߳��� ������״̬: ���� & ����ִ��������
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, 
		[&]() -> bool { return threads_.size() == 0; });
}
// �����̳߳صĹ���ģʽ
void ThreadPool::setMode(ThreadPoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}
// ����task�������������ֵ
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
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool { return taskQue_.size() <    taskQueMaxThreshHold_; }))
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

	// cachedģʽ,������ȽϽ��� ����: С���������,��Ҫ�������������Ϳ����̵߳�����,�ж��Ƿ���Ҫ�����߳�
	if (poolMode_ == ThreadPoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreshHold_)
	{
		std::cout << "crate new thread... threaid:" << std::this_thread::get_id()
			<< " exit!" << std::endl;

		//�������߳�
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)); // bind������Ҫ�󶨳�Ա�����ͳ�Աָ��(Ĭ�ϲ���)
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//�����߳� 
		threads_[threadId]->start(); 
		//�޸��̸߳�����صı���
		curThreadSize_++; // ��Ȼ threads_.size() ���Դ����߳�����,����vector�����̰߳�ȫ��
		idleThreadSize_++; //�����߳�����
	}

	return Result(sp);
}

// �����̳߳�
void ThreadPool::start(int initThreadSize)
{
	//�����̳߳ص�����״̬
	isPoolRunning_ = true;

	// ��¼��ʼ�̸߳���
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;
	// �����̶߳���
	for (int i = 0; i < initThreadSize_; ++i)
	{
		// ����thread�̶߳�����ʺϣ����̺߳�������thread�̶߳���
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)); // bind������Ҫ�󶨳�Ա�����ͳ�Աָ��(Ĭ�ϲ���)
		// unique_ptr ����������ֵ��ֻ�ܽ�����ֵ������Դת��
		//threads_.emplace_back(std::move(ptr));
		int threadId = ptr->getId();
		threads_.emplace(threadId,std::move(ptr));
		//_threads.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));
	}
	// ���������߳�  std::vector<Thread*> _threads;
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threads_[i]->start(); //ָ���̺߳���
		idleThreadSize_++; //��¼��ʼ�����̵߳�����
	}

}

// �����̺߳���  �̳߳ص������̴߳�������������������� 
void ThreadPool::threadFunc(int threadId)
{
	// ��¼ÿ���̵߳���ʼʱ��
	auto lastTime = std::chrono::high_resolution_clock().now();
	while(1)
	{
		std::shared_ptr<Task> task;
		// ��֤������л�ȡ��ȫ, ��ȡ�����Ϳ��԰����ͷŵ�
		{
			// �Ȼ�ȡ��, �����������ͻᱻ�ͷŵ�
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id()
				<< "���Ի�ȡ����..." << std::endl;

			// cachedģʽ��,�п����Ѿ������˺ܶ���߳�,���ǿ���ʱ�䳬��60s,Ӧ�ðѶ�����߳̽������յ�(����initThreadSize_�������߳�Ҫ���л���)
			//��ǰʱ�� - �ϴ��߳�ִ�е�ʱ�� > 60s
			//ÿ���ӷ���һ��, ��ô����: ��ʱ����? �����������ִ�з���
				//��������������Ϊ0��ʼ׼�������߳���Դ
			while (taskQue_.size() == 0)
			{
				// �̳߳�Ҫ����,�����߳���Դ
				if (!isPoolRunning_)
				{
					threads_.erase(threadId);
					std::cout << "threadid:" << std::this_thread::get_id()
						<< " exit" << std::endl;
					exitCond_.notify_all(); //�̺߳�������,�߳̽���
					return;
				}
				
				if (poolMode_ == ThreadPoolMode::MODE_CACHED)
				{
					// ����������ʱ������
					if (std::cv_status::timeout ==
						notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_)
						{
							//��ʼ���յ�ǰ�߳�
							//��¼�߳���������ر�����ֵ�޸�
							//���̶߳�����߳��б�������ɾ��, û�а취 threadFun <==> thread���� ,��Ҫ��thread�������������
							// threadId ==> thread���� ==> ɾ��
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
					// �ȴ�notEmpty����
					notEmpty_.wait(lock);
				}

				//// �̳߳�Ҫ����,�����߳���Դ
				//if (!isPoolRunning_)
				//{
				//	threads_.erase(threadId);
				//	std::cout << "threadid:" << std::this_thread::get_id()
				//		<< " exit" << std::endl;
				//	exitCond_.notify_all();
				//	return;
				//}
			}
			
			idleThreadSize_--; //��¼��ʼ�����̵߳�����

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
		idleThreadSize_++; //��¼��ʼ�����̵߳�����
		lastTime = std::chrono::high_resolution_clock().now(); // ������ʼʱ��
	}
}
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}

///////////////////// �̷߳���ʵ�� /////////////////////
int Thread::generateId_ = 0;
// �̹߳���
Thread::Thread(ThreadFunc func)
	: func_(func)
	, threadId_(generateId_++)
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
	std::thread t(func_,threadId_); // C++11��˵ �̶߳���t �� �̺߳���_func
	t.detach(); // ���÷����߳�  pthread_detach pthread_t 
}
int Thread::getId() const
{
	return threadId_;
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
		result_->setVal( run() ); //���﷢����̬����
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

