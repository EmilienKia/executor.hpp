# tests

TESTS = tests

check_PROGRAMS = $(TESTS)

tests_SOURCES = simple.cpp schedule.cpp scheduletp.cpp runner.cpp
tests_CXXFLAGS = -I../src/
tests_LDFLAGS =  -pthread

noinst_PROGRAMS = test
test_SOURCES = test.cpp
test_CXXFLAGS = -I../src/
test_LDFLAGS =  -pthread
