#pragma once
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cassert>

#include <queue>
#include <vector>
#include <Windows.h>



#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif

class JobSystem;

//Base class for all jobs to be pushed to the system, uses a semaphore to keep track of if it has been completed.
class Job
{
public:
	Job(JobSystem* jobSys)
		:pJobSystem(jobSys), mJobsemaphore(CreateSemaphoreA(nullptr, 0, 1, nullptr)
	{
		//jobsemaphore = CreateSemaphoreA(nullptr, 0, 1, nullptr);
	}

	virtual void operator ()() = 0;

	void Signal() {
		ReleaseSemaphore(jobsemaphore, 1, nullptr);
	}
	void Wait() {
		WaitForSingleObject(jobsemaphore, INFINITE);
	}

	HANDLE		mJobsemaphore;
	JobSystem*	pJobSystem;
};

//This delegate job inherits from job. Uses void pointers to allow for flexibility in usage.
class JobDelegate : public Job
{
public:
	JobDelegate(JobSystem* jobSys) : Job(jobSys), mfptr(nullptr), mParam(nullptr) {}
	JobDelegate(JobSystem* jobSys, void(*f)(void*), void* p) : Job(jobSys), mfptr(f), mParam(p) {}

	void operator()();

	void(*mfptr)(void*);
	void* mParam;
};

class JobSystem
{
public:
	JobSystem(uint32_t worker_thread_count);
	~JobSystem(void);

	void add_job(Job* j);
	void lock_mutex();
	void unlock_mutex();

	void waitforcomplete();


private:
	void do_worker_thread(void);
	static DWORD __stdcall worker_thread(void* user_data);

	uint32_t			thread_count;
	HANDLE*				threads;
	HANDLE				semaphore;
	SRWLOCK				mutex;
	SRWLOCK				jobmutex;
	std::queue<Job*>	job_queue;
	bool				thread_shutdown;
};

JobSystem::JobSystem(uint32_t worker_thread_count) :
	thread_count(worker_thread_count),
	thread_shutdown(false)
{
	semaphore = CreateSemaphoreA(nullptr, 0, LONG_MAX, "Job Queue");
	InitializeSRWLock(&mutex);
	InitializeSRWLock(&jobmutex);

	threads = (HANDLE*)malloc(sizeof(HANDLE) * thread_count);

	for (uint64_t i = 0; i < thread_count; ++i)
		threads[i] = CreateThread(nullptr, 0, worker_thread, this, 0, nullptr);
}

JobSystem::~JobSystem(void)
{
	thread_shutdown = true;
	ReleaseSemaphore(semaphore, thread_count, nullptr);

	for (uint64_t i = 0; i < thread_count; ++i)
	{
		// Join
		WaitForSingleObject(threads[i], INFINITE);
		CloseHandle(threads[i]);
	}

	CloseHandle(semaphore);
	free(threads);
}

void JobSystem::add_job(Job* j)
{
	AcquireSRWLockExclusive(&mutex);
	job_queue.push(j);
	ReleaseSRWLockExclusive(&mutex);
  
	ReleaseSemaphore(semaphore, 1, nullptr);
}
void JobSystem::lock_mutex() {
	AcquireSRWLockExclusive(&jobmutex);
}
void JobSystem::unlock_mutex() {
	ReleaseSRWLockExclusive(&jobmutex);
}

void JobSystem::waitforcomplete() {
	for (;;) 
	{
		AcquireSRWLockExclusive(&mutex);
		if (job_queue.empty()) {
			ReleaseSRWLockExclusive(&mutex);
			return;
		}
		ReleaseSRWLockExclusive(&mutex);
	}
}

void JobSystem::do_worker_thread(void)
{
	for (;;)
	{
		WaitForSingleObject(semaphore, INFINITE);

		Job* job = nullptr;
		AcquireSRWLockExclusive(&mutex);
		if (!job_queue.empty())
		{
			job = job_queue.front();
			job_queue.pop();
		}
		ReleaseSRWLockExclusive(&mutex);

		// Do Job
		if (job)
			(*job)();
		else if (thread_shutdown)
			break;
		else
			assert(false);
	}
}

DWORD JobSystem::worker_thread(void* user_data)
{
	JobSystem* system = (JobSystem*)user_data;
	system->do_worker_thread();

	return 0;
}


void JobDelegate::operator()()
{
	mfptr(mParam);
}