#pragma once
#include <queue>

class IntDelegate
{
public:
	void operator ()() { fptr(param); }
	void(*fptr)(int*);
	int* param;
};

enum class Priority {HIGH, LOW};

class STJobSys
{
public:
	STJobSys() :HighPriority(), LowPriority() {}
	void AddJob(IntDelegate delegate, Priority p) { p == Priority::HIGH ? HighPriority.push(delegate) : LowPriority.push(delegate); }
	void RunJobs() 
	{
		for (;!HighPriority.empty();)
		{
			HighPriority.front()();
			HighPriority.pop();
		}
		for (;!LowPriority.empty();)
		{
			LowPriority.front()();
			LowPriority.pop();
		}
	}

private:
	std::queue<IntDelegate> HighPriority;
	std::queue<IntDelegate> LowPriority;
};