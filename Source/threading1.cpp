#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <queue>
#include <vector>
#include <Windows.h>
#include <assert.h>
#include <cstdlib>

class job_system;

//My class using semaphores
class job
{
public:
	job(job_system* jobSys)
		:jobSystem(jobSys)
	{
		jobsemaphore = CreateSemaphoreA(nullptr, 0, 1, nullptr);
	}


	virtual void do_job(void) = 0;
	job_system* jobSystem;

	void Signal() {
		ReleaseSemaphore(jobsemaphore, 1, nullptr);
	}
	void Wait() {
		WaitForSingleObject(jobsemaphore, INFINITE);
	}

	HANDLE				jobsemaphore;
};

//Aaron's using events
//class job
//{
//public:
//	job(job_system* jobSys)
//		:jobSystem(jobSys)
//	{
//		event = CreateEventA(nullptr, false, false, "Complete Job");
//	}
//
//	virtual ~job(void) {
//		CloseHandle(event);
//	}
//
//	virtual void do_job(void) = 0;
//
//	void Signal() {
//		SetEvent(event);
//	}
//	void Wait() {
//		WaitForSingleObject(event, INFINITE);
//	}
//
//	job_system* jobSystem;
//	HANDLE        event;
//};


class job_addtonumbers : public job
{
public:
	job_addtonumbers(job_system* jobSys, std::queue<double>* numbers) :
		job(jobSys), numbers(numbers)
	{
	}

	void do_job(void) override;

	std::queue<double>* numbers;
};

class job_reducetoaverage : public job
{
public:
	job_reducetoaverage(job_system* jobSys, std::queue<double>* numbers) :
		job(jobSys), numbers(numbers)
	{
	}

	void do_job(void) override;

	std::queue<double>* numbers;
};

class job_system
{
public:
	job_system(uint32_t worker_thread_count);
	~job_system(void);

	void add_job(job* j);
	void lock_mutex();
	void unlock_mutex();

	void waitforcomplete();


private:
	void do_worker_thread(void);
	static DWORD worker_thread(void* user_data);

	uint32_t			thread_count;
	HANDLE*				threads;
	HANDLE				semaphore;
	SRWLOCK				mutex;
	SRWLOCK				jobmutex;
	std::queue<job*>	job_queue;
	bool				thread_shutdown;
};

job_system::job_system(uint32_t worker_thread_count) :
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

job_system::~job_system(void)
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

void job_system::add_job(job* j)
{
	AcquireSRWLockExclusive(&mutex);
	job_queue.push(j);
	ReleaseSRWLockExclusive(&mutex);

	ReleaseSemaphore(semaphore, 1, nullptr);
}
void job_system::lock_mutex() {
	AcquireSRWLockExclusive(&jobmutex);
}
void job_system::unlock_mutex() {
	ReleaseSRWLockExclusive(&jobmutex);
}

void job_system::waitforcomplete() {
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

void job_system::do_worker_thread(void)
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
			job->do_job();
		else if (thread_shutdown)
			break;
		else
			assert(false);
	}
}

DWORD job_system::worker_thread(void* user_data)
{
	job_system* system = (job_system*)user_data;
	system->do_worker_thread();

	return 0;
}


void job_addtonumbers::do_job(void)
{
	printf(
		"job_increase: %d, I want ++++++++!\n",
		GetCurrentThreadId()
	);

	jobSystem->lock_mutex();

	numbers->push((double)rand() / RAND_MAX);
	jobSystem->unlock_mutex();

	job::Signal();
}

void job_reducetoaverage::do_job(void)
{
	printf(
		"job_reduce: %d, I want ---------!\n",
		GetCurrentThreadId()
	);

	jobSystem->lock_mutex();

	double n1 = numbers->front();
	numbers->pop();
	double n2 = numbers->front();
	numbers->pop();

	double r = n1 + n2;
	
	numbers->push(r);
	jobSystem->unlock_mutex();

	job::Signal();
}


int main(void)
{
	int numbersCount = 200;

	std::queue<double> numbers;
	
	std::vector<job*> addjobs;
	std::vector<job*> reducejobs;

	addjobs.reserve(numbersCount);

	reducejobs.reserve(numbersCount-1);

	job_system* jobSystem = new job_system(8);

	for (int i = 0; i < numbersCount; ++i) {
		addjobs.push_back(new job_addtonumbers(jobSystem, &numbers));

		jobSystem->add_job(addjobs[i]);
	}

	for (int w = 0; w < numbersCount; ++w) {
		addjobs[w]->Wait();
	}
	
	//Average Check using basic method
	/*double mainAverage(0);
	std::queue<double>mainNumbers(numbers);
	for (int i = 0; i < numbersCount; ++i) {
		mainAverage += mainNumbers.front();
		mainNumbers.pop();
	}
	mainAverage = mainAverage /numbersCount;


	//Average check which mimics threads method
	double mainAverage2(0);
	std::queue<double>mainNumbers2(numbers);
	for (int i = 0; i < numbersCount-1; ++i) {
		double n1 = mainNumbers2.front();
		mainNumbers2.pop();
		double n2 = mainNumbers2.front();
		mainNumbers2.pop();

		double r = n1 + n2;

		mainNumbers2.push(r);
	}
	mainAverage2 = mainNumbers2.front();
	mainAverage2 = mainAverage2 / numbersCount;*/


	for (int i = 0; i < numbersCount-1; ++i) {
		reducejobs.push_back(new job_reducetoaverage(jobSystem, &numbers));

		jobSystem->add_job(reducejobs[i]);
	}

	delete jobSystem;

	for (int i = 0; i < addjobs.size(); ++i)
		delete addjobs[i];
	for (int i = 0; i < reducejobs.size(); ++i)
		delete reducejobs[i];

	double numbersAverage = numbers.front() / numbersCount;

	printf("\nAVERAGE : %f\n", numbersAverage);

	//Average check prints
	//printf("\nMAIN AVERAGE CHECK: %f\n", mainAverage);
	//printf("\nMAIN AVERAGE CHECK: %f\n", mainAverage2);

	system("pause");
}