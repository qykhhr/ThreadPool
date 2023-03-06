
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

	Any run() // run方法最终就在线程池分配的线程中去做事情了
	{
		std::cout << "tid:" << std::this_thread::get_id()
			<< "begin!" << std::endl;
		//std::this_thread::sleep_for(std::chrono::seconds(9));

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
	ThreadPool pool;
	pool.start(4);
	
	Result res1 = pool.submitTask(std::make_shared<MyTask>(1,100000000));
	Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
	Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	
	// 随着task被执行完,task对象没了,依赖于task对象的Result对象也没了
	ULong sum1 = res1.get().cast_<ULong>();
	ULong sum2 = res2.get().cast_<ULong>();
	ULong sum3 = res3.get().cast_<ULong>();

	/*
	Master - Slave 线程模型
	Master线程用来分解线程, 然后给各个Salve线程分配任务
	等待各个Slave线程执行完任务,返回结果
	Master线程合并各个任务结果,输出
	*/
	std::cout << (sum1 + sum2 + sum3) << std::endl;

	//std::this_thread::sleep_for(std::chrono::seconds(5)); 
	getchar();
} 