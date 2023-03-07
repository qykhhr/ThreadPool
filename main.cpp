
#include<iostream>
#include<chrono>
#include<thread>
#include "threadpool.h"

using ULong = unsigned long long;

class MyTask : public Task
{
public:
	MyTask(int begin, int end)
		: begin_(begin)
		, end_(end)
	{
	}

	Any run() // run�������վ����̳߳ط�����߳���ȥ��������
	{
		std::cout << "tid:" << std::this_thread::get_id()
			<< "begin!" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(6));

		ULong sum = 0;

		for (int i = begin_; i <= end_; i++)
			sum += i;

		std::cout << "tid:" << std::this_thread::get_id()
			<< "end!" << std::endl;

		return sum;
	} 
private:
	int begin_;
	int end_;
};

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

	std::cout << "main over" << std::endl;
	getchar();

#if 0
	{
		ThreadPool pool;

		// �û��Լ������̳߳صĹ���ģʽ
		pool.setMode(ThreadPoolMode::MODE_CACHED);

		pool.start(4);

		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Result res4 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Result res5 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Result res6 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

		// ����task��ִ����,task����û��,������task�����Result����Ҳû��
		ULong sum1 = res1.get().cast_<ULong>();
		ULong sum2 = res2.get().cast_<ULong>();
		ULong sum3 = res3.get().cast_<ULong>();

		/*
		Master - Slave �߳�ģ��
		Master�߳������ֽ��߳�, Ȼ�������Salve�̷߳�������
		�ȴ�����Slave�߳�ִ��������,���ؽ��
		Master�̺߳ϲ�����������,���
		*/
		std::cout << (sum1 + sum2 + sum3) << std::endl;
	}

	//std::this_thread::sleep_for(std::chrono::seconds(5)); 
	getchar();

#endif
} 