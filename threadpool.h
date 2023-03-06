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


//�ź�����Դ�� , ���Կ��߳̽���ͨ��
class Semaphore
{
public:
	Semaphore(int limit = 0);
	~Semaphore() = default;

	// ��ȡһ���ź�����Դ
	void wait();
	// ����һ���ź�����Դ
	void post();
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};


// Any����: ���Խ����������ݵ�����
class Any
{
public:
	Any() = default;
	~Any() = default;

	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;

	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	// ������캯��������Any���ͽ�����������������
	template<typename T>
	Any(T data)
		: base_(std::make_unique<Derive<T>>(data))
	{

	}

	// ��������ܰ�Any��������洢��data������ȡ����
	template<typename T>
	T cast_()
	{
		// ������ô��base_�ҵ�����ָ���Derive����,��������ȡ��data��Ա����
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type id unmatch!";
		}
		return pd->data_;
	}
private:
	// ��������
	class Base
	{
	public:
		virtual ~Base() = default;
	};
	// ����������
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
	// ����һ������ָ�� 
	std::unique_ptr<Base> base_;
};

class Task;

// ʵ�ֽ����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����Result
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;
	// ����һ: setVal��������ȡ����ִ����ķ���ֵ��
	void setVal(Any res);
	// �����: get �������û��������������ȡtask�ķ���ֵ
	Any get(); // �û����õ�
private:
	Any any_; /// �洢����ķ���ֵ
	Semaphore sem_; // �߳�ͨ���ź���
	std::shared_ptr<Task> task_; // ִ�ж�Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_; // ����ֵ�Ƿ���Ч
};

// ����������
class Task
{
public:
	Task();
	~Task() = default;

	// �û������Զ��������������ͣ���Task�̳У���дrun������ʵ���Զ���������
	virtual Any run() = 0;
	void exec();
	void setResult(Result* res);
private:
	Result* result_;
};


// �̳߳�֧�ֵ�ģʽ
enum class ThreadPoolMode // ���� class ����ʹ�� ThreadPoolMode::MODE_FIXED ��ѡ�ã�������ö������������ͬ������ͻ
{
	MODE_FIXED, // �̶��������̳߳�
	MODE_CACHED // �߳������ɶ�̬����
};
// �߳�����
class Thread
{
public:
	// �̺߳�������ģ��
	using ThreadFunc = std::function<void()>;
	
	// �̹߳���
	Thread(ThreadFunc func);
	// �߳�����
	~Thread();
		
	// �����߳�
	void start();

private:
	ThreadFunc _func;
};
// �̳߳�
class ThreadPool
{
public:
	// �̳߳ع���
	ThreadPool();
	// �̳߳�����
	~ThreadPool();

	// �����̳߳صĹ���ģʽ
	void setMode(ThreadPoolMode mode);

	// ����task�������������ֵ
	void setTaskQueMaxThreadHold(int threadHold);
	

	// ���̳߳��ύ����
	Result submitTask(std::shared_ptr<Task> sp);

	// �����̳߳�
	void start(int initThreadSize = 4);

	// ��ֹ����ֵ����������
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// �����̺߳���
	void threadFunc();

private:
	std::vector<std::unique_ptr<Thread>> threads_; // �߳��б�
	size_t initThreadSize_; // ��ʼ���߳�����
	
	// �û����ܴ����������ڶ̵Ķ���(��ʱ�����ָ��)
	std::queue<std::shared_ptr<Task>> taskQue_; // �������
	std::atomic_int taskSize_; // ��������
	size_t taskQueMaxThreadsHold_; // �����������������ֵ, �ȶ�����

	std::mutex taskQueMtx_; // ��֤������е��̰߳�ȫ
	std::condition_variable notFull_; // ��ʾ������в���
	std::condition_variable notEmpty_; // ��ʾ������в���

	ThreadPoolMode poolMode_; // ��ǰ�̳߳صĹ���ģʽ
};

#endif