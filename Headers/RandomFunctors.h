#pragma once
#include <random>
#include <chrono>

class EvenRandGen
{
public:
	std::mt19937 engine;
	int max;
	int min;

	EvenRandGen(int lower, int upper) : min(lower), max(upper)
	{
		auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		engine.seed(unsigned long(seed));
	}

	int operator()()
	{
		std::uniform_int_distribution<int> distribution(min, max/2);
		return(distribution(engine)*2);
	}
};

class RandGen
{
public:
	std::mt19937 engine;
	int max;
	int min;

	RandGen(int lower, int upper) : min(lower), max(upper)
	{
		auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		engine.seed(unsigned long(seed));
	}

	int operator()()
	{
		std::uniform_int_distribution<int> distribution(min, max);
		return(distribution(engine));
	}

};
class OddRandGen
{
public:
	std::mt19937 engine;
	int max;
	int min;

	OddRandGen(int lower, int upper) : min(lower), max(upper)
	{
		auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		engine.seed(unsigned long(seed));
	}

	int operator()()
	{
		std::uniform_int_distribution<int> distribution(min, max / 2);
		return((distribution(engine) * 2) +1);
	}

};