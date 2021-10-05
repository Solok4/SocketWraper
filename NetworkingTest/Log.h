#pragma once
#include <stdio.h>
#include <stdarg.h>
#include <cstring>
#include <mutex>

class CLog
{
public:
	CLog();
	~CLog();

	static void debug(const char* message, ...);
	static void info(const char* message, ...);
	static void error(const char* message, ...);
private:
	static void printLocked(const char* format, std::string data);
};

