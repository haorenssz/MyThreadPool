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

	//�����������Ӳ���
	/*
	*���أ�auto������ͨ��β�÷��ػ�ȡ
	*���������������
	*���ڰѹ�������ѹ��tasks
	*
	*ʹ�÷�����
	*1������ָ�봴������ʵ�壬����ʵ��
	*auto (*F)(int);
	*F=f;//�Ѷ���
	*ThreadPool.enqueue(F);
	*2������lambda������������
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
	//���溯��
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

		//emplace_backԭ�ع��죬ʡ�ռ�
		workers.emplace_back(
			//lambda������ֵΪ������task����
			[this]
			{
				for (;;)
				{
					std::function<void()> task;

					{
						//��mutex,��ʼ���߳��������
						std::unique_lock<std::mutex> lock(this->queue_mutex);
						//�ȵ�
						this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
						//���Ѿ�ͣ�ˣ����п��ˣ�
						if (this->stop && this->tasks.empty())
							return;
						//���ո�task��ֵ
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
	//��δ֪��F����ֵ��������Ϊreturn_type
	using return_type = typename std::result_of<F(Args...)>::type;

	//make_share<...>������shared_ptr ����ָ�룬�����new��shared_ptr��ֻ����һ���������ڴ����ͬʱ��Ŷ������ݺͿ��ƿ�
	//bind�������������ض��Ĳ���	
	auto task = std::make_shared< std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);
	//����һ��futureʵ���ȡ��ǰ���񷵻صĽ��
	std::future<return_type> res = task->get_future();

	{
		std::unique_lock<std::mutex> lock(queue_mutex);

		// don't allow enqueueing after stopping the pool
		if (stop)
			throw std::runtime_error("enqueue on stopped ThreadPool");

		tasks.emplace([task]() { (*task)(); });
	}
	//����
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
	//���������߳�
	condition.notify_all();
	//ȫ������
	for (std::thread& worker : workers)
		worker.join();
}

#endif
