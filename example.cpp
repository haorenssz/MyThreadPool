#include <iostream>
#include "ThreadPool.h"
#include <chrono>
/*
int main()
{
	//��ʼ��һ��6�߳��̳߳�
	ThreadPool pool(6);

	// enqueue and store future
	auto result = pool.enqueue([](int answer) { return answer; }, 42);

	// get result from future, print 42
	std::cout << result.get() << std::endl;
}
*/

void func()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	std::cout<<"worker thread ID:"<<std::this_thread::get_id()<<std::endl;
}

int main()
{
	ThreadPool pool(6);
	while(1)
	{
	   pool.enqueue(func);
	}
}
