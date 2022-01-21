#pragma once
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <queue>
#include <vector>
#include <Windows.h>
#include <assert.h>
#include <cstdlib>

#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif

class JobSystem;

class job
{
public:
	job(JobSystem* jobSys)
		:jobSystem(jobSys)
	{
		jobsemaphore = CreateSemaphoreA(nullptr, 0, 1, nullptr);
	}

	virtual void operator ()() = 0;
	JobSystem* jobSystem;

	void Signal() {
		ReleaseSemaphore(jobsemaphore, 1, nullptr);
	}
	void Wait() {
		WaitForSingleObject(jobsemaphore, INFINITE);
	}

	HANDLE				jobsemaphore;
};

class job_delegate : public job
{
public:
	job_delegate(JobSystem* jobSys) : job(jobSys), fptr(nullptr), param(nullptr) {}
	job_delegate(JobSystem* jobSys, void(*f)(void*), void* p) : job(jobSys), fptr(f), param(p) {}

	void operator()() override;

	void(*fptr)(void*);
	void* param;
};

class JobSystem
{
public:
	JobSystem(uint32_t worker_thread_count);
	~JobSystem(void);

	void add_job(job* j);
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
	std::queue<job*>	job_queue;
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

void JobSystem::add_job(job* j)
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

		job* job = nullptr;
		AcquireSRWLockExclusive(&mutex);
		if (!job_queue.empty())
		{
			job = job_queue.front();
			job_queue.pop();
		}
		ReleaseSRWLockExclusive(&mutex);

		// Do job
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


void job_delegate::operator()()
{
	fptr(param);
}