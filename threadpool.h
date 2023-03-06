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

//�ź�����Դ��
class Semaphore
{
public:
	Semaphore(int limit = 0)
		: _resLimit(limit)
	{

	}
	~Semaphore() = default;

	// ��ȡһ���ź�����Դ
	void wait()
	{
		std::unique_lock<std::mutex> lock(_mtx);
		// �ȴ��ź�������Դ��û����Դ�Ļ�����������ǰ�߳�
		_cond.wait(lock, [&]() -> bool { return _resLimit > 0; });
		_resLimit--;
	}
	// ����һ���ź�����Դ
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


//// Any����: ���Խ����������ݵ�����
//class Any
//{
//public:
//	Any() = default;
//	~Any() = default;
//
//	// ������캯��������Any���ͽ�����������������
//	template<typename T>
//	Any(T data) : _base(std::make_unique<Derive<T>>(data)))
//	{
//		
//	}
//	//��������ܰ�Any��������洢��data������ȡ����
//	template<typename T>
//	T cast()
//	{
//		// ������ô��_base�ҵ�����ָ���Derive���󣬴�������ȡ��data��Ա����
//		// ����ָ�� ==> ������ָ�� RTTI
//		Derive<T>* pd = dynamic_cast<Derive<T>*>(_base.get());
//		if (pd == nullptr)
//		{
//			throw "type is notmatch!";
//		}
//		return pd->_data;
//	}
//private:
//	// ��������
//	class Base
//	{
//	public:
//		~Base() = default;
//	};
//	// ����������
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
//	// ����һ������ָ��
//	std::unique_ptr<Base> _base;
//};


// Task���͵�ǰ������
class Task;

//// ʵ�ֽ����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����Result
//class Result
//{
//public:
//	Result(std::shared_ptr<Task> task, bool isValid = true);
//	~Result() = default;
//	// ����һ: setVal��������ȡ����ִ����ķ���ֵ��
//	void setVal(Result res);
//	// �����: get �������û��������������ȡtask�ķ���ֵ
//	Any get();
//private:
//	Any _any; /// �洢����ķ���ֵ
//	Semaphore _sem; // �߳�ͨ���ź���
//	std::shared_ptr<Task> _task; // ִ�ж�Ӧ��ȡ����ֵ���������
//	std::atomic_bool _isValid; // ����ֵ�Ƿ���Ч
//};

// ����������
class Task
{
public:
	// �û������Զ��������������ͣ���Task�̳У���дrun������ʵ���Զ���������
	virtual void run() = 0;
	void exec();
	//void setResult(Result* res);
private:
	//Result* _result;
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
	void submitTask(std::shared_ptr<Task> sp);

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