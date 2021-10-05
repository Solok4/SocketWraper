#include "Log.h"

CLog::CLog()
{
}


CLog::~CLog()
{
}

void CLog::debug(const char* message, ...) {
	char VAContent[2048];

	va_list vl;
	va_start(vl, message);
	vsprintf(VAContent, message, vl);
	std::string finalMessage = "[DEBUG]: ";
	finalMessage += VAContent;
	CLog::printLocked("%s\n", finalMessage);
	va_end(vl);
}

void CLog::info(const char* message, ...) {
	char VAContent[2048];

	va_list vl;
	va_start(vl, message);
	vsprintf(VAContent, message, vl);
	std::string finalMessage = "[INFO]: ";
	finalMessage += VAContent;
	CLog::printLocked("%s\n", finalMessage);
	va_end(vl);
}

void CLog::error(const char* message, ...){
	char VAContent[2048];

	va_list vl;
	va_start(vl, message);
	vsprintf(VAContent, message, vl);
	std::string finalMessage = "[ERROR]: ";
	finalMessage += VAContent;
	CLog::printLocked("%s\n", finalMessage);
	va_end(vl);
}

void CLog::printLocked(const char* format, std::string data){
	static std::mutex printMutex;
	std::lock_guard<std::mutex> lockGuard(printMutex);
	printf(format,data.c_str());
}