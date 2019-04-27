#pragma once
#include<stdio.h>
#include<stdarg.h>
#include<cstring>

enum LogTag
{
	Log=0,
	Warning,
	Error,
};

class Clog
{
public:
	Clog();
	~Clog();
	static void PrintLog(LogTag tag,const char* format, ...);
};

