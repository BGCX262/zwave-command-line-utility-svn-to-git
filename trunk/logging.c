/************************************************************************************
*
*		logging.c
*
*		Implementation of a logging function
*
*************************************************************************************/

#include "logging.h"

FILE *logfileHandle;
int logLevel;

void logInit(char* logfile) {  // Open log file and store handle

    if (logLevel < 5) {
	logfileHandle = fopen(logfile, "a");
    	if (logfileHandle < 0) {
	    printf("Error opening logfile: %s\n.  Exiting.",logfile);
	    exit(EXIT_FAILURE);
	}
    }
}

void logClose(void) {  // Close the log file

    if (logfileHandle > 0)
    	fclose(logfileHandle);
}

void logInfo(int logtype, char *logline) {

    time_t ltime; /* calendar time */
    char *timestring;
    ltime=time(NULL); /* get current cal time */

    timestring = asctime(localtime(&ltime));
    timestring[strlen(timestring)-1] = '\0';

    if (logfileHandle > 0)
	if (logtype <= logLevel) {
		fprintf(logfileHandle,"%s:  %s",timestring,logline);
		fflush(logfileHandle);
	}

    if (logLevel == 5)
	printf("%s:  %s",timestring,logline);


}

