#pragma once
#include <cassert>
#include <vector>
#include <thread>
#include <semaphore>
#include <mutex>
#include <queue>

class Job
{
public:
	Job(void(*func)(void*), void* param)
		:m_func(func), m_param(param), m_semaphore(0)
	{

	}

	Job(const Job& j)
		:m_func(j.m_func), m_param(j.m_param), m_semaphore(0)
	{}

	void operator()()
	{
		m_func(m_param);
		m_semaphore.release();
	}

	void waitForComplete()
	{
		m_semaphore.acquire();
	}
private:
	void(*m_func)(void*);
	void* m_param;
	std::binary_semaphore m_semaphore;
};


template<int MaxJobs>
class ThreadPool
{

	void doWork()
	{
		for (;;)
		{
			while (!m_queueSemaphore.try_acquire());

			Job* j = nullptr;
			while (!m_queueMutex.try_lock());
			if (!m_jobs.empty())
			{
				j = m_jobs.front();
				m_jobs.pop();
			}
			m_queueMutex.unlock();

			if (j)
				(*j)();
			else if (m_shutdown)
				break;
			else
				assert(false);
		}
	}

	static void threadStart(ThreadPool* pool)
	{
		pool->doWork();
	}

	std::vector<std::thread> m_vThreads;
	std::queue<Job*> m_jobs;
	std::mutex m_queueMutex;
	std::counting_semaphore<MaxJobs> m_queueSemaphore;
	bool m_shutdown;

public:
	ThreadPool(int threadCount)
		: m_vThreads(0), m_queueSemaphore(0), m_shutdown(false)
	{
		for (int i = 0; i < threadCount; ++i)
		{
			m_vThreads.emplace_back(std::thread(threadStart, this));
		}
	}

	ThreadPool() = delete;

	~ThreadPool()
	{
		m_shutdown = true;
		m_queueSemaphore.release(m_vThreads.size());
		for (std::thread& t : m_vThreads)
		{
			t.join();
		}
	}

	void AddJob(Job* j)
	{
		while (!m_queueMutex.try_lock());
		m_jobs.push(j);
		m_queueMutex.unlock();
		m_queueSemaphore.release();
	}

	size_t GetThreadCount() const { return m_vThreads.size(); }
};