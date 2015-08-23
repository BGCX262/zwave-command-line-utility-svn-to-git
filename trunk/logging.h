/************************************************************************************
*
*		logging.h
*
*		Implementation of a logging function
*
*************************************************************************************/

#ifndef _LOGGING_H_
#define _LOGGING_H_

#define LOG_ERROR		1
#define LOG_WARN			2
#define LOG_INFO			3
#define LOG_ALL			4

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

void logInit(char* logfile);  // Open log file and store handle
void logClose(void);  // Close the log file
void logInfo(int logtype, char *logline);


#endif
