/* -*- Mode: CPP; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * tests/scheduletp.cpp
 * Copyright (C) 2018 Emilien Kia <emilien.kia+dev@gmail.com>
 */

#include "catch.hpp"

#include "executor.hpp"

#include <iostream>

//
// System clock based tests:
//

TEST_CASE( "Thread pool schedule executor", "[executor][schedule][thread_pool][system_clock]" )
{
	bool invoked = false;
	std::chrono::system_clock::time_point tp;
	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds epsillon {100};
	exec::thread_pool_schedule_executor<std::chrono::system_clock> ex(2);

	std::chrono::system_clock::time_point before = std::chrono::system_clock::now();

	auto run = [&](){
		invoked = true;
		tp = std::chrono::system_clock::now();
	};

	auto task = ex.schedule(run, wait);

	task->get_future().wait();

	CHECK( invoked );
	CHECK( tp >= before + wait );
	CHECK( tp < before + wait + epsillon );
}

TEST_CASE( "Thread pool rate schedule executor", "[executor][schedule][thread_pool][system_clock]" )
{
	long count = 3;
	long invoked = 0;
	std::vector<std::chrono::system_clock::time_point> tp;

	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds epsillon {100};
	exec::thread_pool_schedule_executor<std::chrono::system_clock> ex(2);

	std::chrono::system_clock::time_point before = std::chrono::system_clock::now();

	auto run = [&](){
		invoked++;
		tp.push_back(std::chrono::system_clock::now());
	};

	auto task = ex.schedule_rate(run, wait, wait, count);

	std::this_thread::sleep_for( wait + wait*count + wait);

	CHECK( task->count() == count );
	CHECK( task->remaining() == 0 );

	CHECK( invoked == count );
	CHECK( tp.size() == count );
	for(size_t n=0; n<count; ++n)
	{
		CHECK( tp[n] >= before + wait + wait*n );
		CHECK( tp[n] <  before + wait + wait*n + epsillon );
	}
}


TEST_CASE( "Thread pool delay schedule executor", "[executor][schedule][thread_pool][system_clock]" )
{
	long count = 3;
	long invoked = 0;
	std::vector<std::chrono::system_clock::time_point> tp;

	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds pause   { 500};
	std::chrono::milliseconds epsillon {100};
	exec::thread_pool_schedule_executor<std::chrono::system_clock> ex(2);

	std::chrono::system_clock::time_point before = std::chrono::system_clock::now();

	auto run = [&](){
		invoked++;
		tp.push_back(std::chrono::system_clock::now());
		std::this_thread::sleep_for(pause);
	};

	auto task = ex.schedule_delay(run, wait, wait, count);

	std::this_thread::sleep_for( wait + (wait+pause)*count + wait);

	CHECK( task->count() == count );
	CHECK( task->remaining() == 0 );

	CHECK( invoked == count );
	CHECK( tp.size() == count );
	for(size_t n=0; n<count; ++n)
	{
		CHECK( tp[n] >= before + wait + (wait+pause)*n );
		CHECK( tp[n] <  before + wait + (wait+pause)*n + epsillon );
	}
}



//
// Steady clock based tests:
//


TEST_CASE( "Steady Thread pool schedule executor", "[executor][schedule][thread_pool][steady_clock]" )
{
	bool invoked = false;
	std::chrono::steady_clock::time_point tp;
	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds epsillon {100};
	exec::thread_pool_schedule_executor<std::chrono::steady_clock> ex(2);

	std::chrono::steady_clock::time_point before = std::chrono::steady_clock::now();

	auto run = [&](){
		invoked = true;
		tp = std::chrono::steady_clock::now();
	};

	auto task = ex.schedule(run, wait);

	task->get_future().wait();

	CHECK( invoked );
	CHECK( tp >= before + wait );
	CHECK( tp < before + wait + epsillon );
}

TEST_CASE( "Steady Thread pool rate schedule executor", "[executor][schedule][thread_pool][steady_clock]" )
{
	long count = 3;
	long invoked = 0;
	std::vector<std::chrono::steady_clock::time_point> tp;

	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds epsillon {100};
	exec::thread_pool_schedule_executor<std::chrono::steady_clock> ex(2);

	std::chrono::steady_clock::time_point before = std::chrono::steady_clock::now();

	auto run = [&](){
		invoked++;
		tp.push_back(std::chrono::steady_clock::now());
	};

	auto task = ex.schedule_rate(run, wait, wait, count);

	std::this_thread::sleep_for( wait + wait*count + wait);

	CHECK( task->count() == count );
	CHECK( task->remaining() == 0 );

	CHECK( invoked == count );
	CHECK( tp.size() == count );
	for(size_t n=0; n<count; ++n)
	{
		CHECK( tp[n] >= before + wait + wait*n );
		CHECK( tp[n] <  before + wait + wait*n + epsillon );
	}
}


TEST_CASE( "Steady Thread pool delay schedule executor", "[executor][schedule][thread_pool][steady_clock]" )
{
	long count = 3;
	long invoked = 0;
	std::vector<std::chrono::steady_clock::time_point> tp;

	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds pause   { 500};
	std::chrono::milliseconds epsillon {100};
	exec::thread_pool_schedule_executor<std::chrono::steady_clock> ex(2);

	std::chrono::steady_clock::time_point before = std::chrono::steady_clock::now();

	auto run = [&](){
		invoked++;
		tp.push_back(std::chrono::steady_clock::now());
		std::this_thread::sleep_for(pause);
	};

	auto task = ex.schedule_delay(run, wait, wait, count);

	std::this_thread::sleep_for( wait + (wait+pause)*count + wait);

	CHECK( task->count() == count );
	CHECK( task->remaining() == 0 );

	CHECK( invoked == count );
	CHECK( tp.size() == count );
	for(size_t n=0; n<count; ++n)
	{
		CHECK( tp[n] >= before + wait + (wait+pause)*n );
		CHECK( tp[n] <  before + wait + (wait+pause)*n + epsillon );
	}
}
