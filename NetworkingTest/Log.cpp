#include "pch.h"
#include "Log.h"


Clog::Clog()
{
}


Clog::~Clog()
{
}

void Clog::PrintLog(LogTag tag,const char* format, ...)
{

	char result[2048];
	char VAContent[2048];
	switch (tag)
	{
	case Log:
		strncpy(result, "[Log]: ", 8);
		break;
	case Warning:
		strncpy(result, "[Warning]: ", 12);
		break;
	case Error:
		strncpy(result, "[Error]: ", 10);
		break;
	default:

		break;
	}

	va_list list;
	va_start(list, format);
	vsprintf(VAContent, format, list);
	strcat(result, VAContent);
	printf("%s\n",result);

	va_end(list);
}
