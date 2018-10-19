/* -*- Mode: CPP; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * tests/schedule.cpp
 * Copyright (C) 2018 Emilien Kia <emilien.kia+dev@gmail.com>
 */

#include "catch.hpp"

#include "executor.hpp"

#include <iostream>

//
// System clock based tests:
//

TEST_CASE( "Thread schedule executor", "[executor][schedule][system_clock]" )
{
	bool invoked = false;
	std::chrono::system_clock::time_point tp;
	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds epsillon {100};
	exec::thread_schedule_executor<std::chrono::system_clock> ex;

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

TEST_CASE( "Thread rate schedule executor", "[executor][schedule][system_clock]" )
{
	long count = 3;
	long invoked = 0;
	std::vector<std::chrono::system_clock::time_point> tp;

	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds epsillon {100};
	exec::thread_schedule_executor<std::chrono::system_clock> ex;

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


TEST_CASE( "Thread delay schedule executor", "[executor][schedule][system_clock]" )
{
	long count = 3;
	long invoked = 0;
	std::vector<std::chrono::system_clock::time_point> tp;

	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds pause   { 500};
	std::chrono::milliseconds epsillon {100};
	exec::thread_schedule_executor<std::chrono::system_clock> ex;

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


TEST_CASE( "Steady Thread schedule executor", "[executor][schedule][steady_clock]" )
{
	bool invoked = false;
	std::chrono::steady_clock::time_point tp;
	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds epsillon {100};
	exec::thread_schedule_executor<std::chrono::steady_clock> ex;

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

TEST_CASE( "Steady Thread rate schedule executor", "[executor][schedule][steady_clock]" )
{
	long count = 3;
	long invoked = 0;
	std::vector<std::chrono::steady_clock::time_point> tp;

	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds epsillon {100};
	exec::thread_schedule_executor<std::chrono::steady_clock> ex;

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


TEST_CASE( "Steady Thread delay schedule executor", "[executor][schedule][steady_clock]" )
{
	long count = 3;
	long invoked = 0;
	std::vector<std::chrono::steady_clock::time_point> tp;

	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds pause   { 500};
	std::chrono::milliseconds epsillon {100};
	exec::thread_schedule_executor<std::chrono::steady_clock> ex;

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
