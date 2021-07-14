#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
	ThreadPool(size_t);

	//工作函数进队操作
	/*
	*返回：auto，类型通过尾置返回获取
	*传入参数：函数体
	*用于把工作函数压入tasks
	*
	*使用方法：
	*1、函数指针创建函数实体，传入实体
	*auto (*F)(int);
	*F=f;//已定义
	*ThreadPool.enqueue(F);
	*2、传入lambda或者匿名函数
	*ThreadPool.enqueue([],{......;return a;});
	*/
	template<class F, class... Args>
	auto enqueue(F&& f, Args&& ... args)
		->std::future<typename std::result_of<F(Args...)>::type>;

	~ThreadPool();

private:
	// need to keep track of threads so we can join them
	std::vector< std::thread > workers;

	// the task queue
	//储存函数
	std::queue< std::function<void()> > tasks;

	// synchronization
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
	: stop(false)
{
	for (size_t i = 0; i < threads; ++i)

		//emplace_back原地构造，省空间
		workers.emplace_back(
			//lambda，返回值为函数，task（）
			[this]
			{
				for (;;)
				{
					std::function<void()> task;

					{
						//锁mutex,开始做线程里的任务
						std::unique_lock<std::mutex> lock(this->queue_mutex);
						//先等
						this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
						//判已经停了？队列空了？
						if (this->stop && this->tasks.empty())
							return;
						//不空给task赋值
						task = std::move(this->tasks.front());
						this->tasks.pop();
					}

					task();
				}
			}
			);
}

// add new work item to the pool

template<class F, class... Args>
auto ThreadPool::enqueue(F && f, Args && ... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
	//将未知的F返回值类型声明为return_type
	using return_type = typename std::result_of<F(Args...)>::type;

	//make_share<...>来构造shared_ptr 智能指针，相比用new，shared_ptr能只申请一个单独的内存块来同时存放对象内容和控制块
	//bind（）给函数绑定特定的参数	
	auto task = std::make_shared< std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);
	//创建一个future实体获取当前任务返回的结果
	std::future<return_type> res = task->get_future();

	{
		std::unique_lock<std::mutex> lock(queue_mutex);

		// don't allow enqueueing after stopping the pool
		if (stop)
			throw std::runtime_error("enqueue on stopped ThreadPool");

		tasks.emplace([task]() { (*task)(); });
	}
	//唤醒
	condition.notify_one();
	return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{

	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		stop = true;
	}
	//唤醒所有线程
	condition.notify_all();
	//全部放走
	for (std::thread& worker : workers)
		worker.join();
}

#endif
