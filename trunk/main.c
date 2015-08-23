/************************************************************************************
*
*		main.c
*
*		Z-Wave utility.  Implements on, off, and level.
*
*************************************************************************************/

#include "main.h"
#include "logging.h"
#include "serial.h"
#include "ZWCommands.h"
#include "ZWApi.h"

//	Global variables
int option, v_probeflag, v_initflag, v_keepalive;		//Command line arguments
int logLevel, nodeID, nodeLevel;   //Node ID and level setting
char *zwdevice;  //Hardware device for controller
int zwQueueIndex;

int main(int argc, char *argv[])
{
//	Set defaults
	zwdevice = "/dev/ttyUSB0";
	logLevel = 4;	//Log all by default
	v_initflag = 0;
	v_probeflag = 0;
	nodeID=0;
	nodeLevel=0;

	while ((option = getopt (argc, argv, "d:l:n:s:pik")) != -1) {
		switch (option) {
		case 'p':
			v_probeflag = 1;
			break;
		case 'i':
			v_initflag = 1;
			break;
		case 'k':
			v_keepalive = 1;
			break;
		case 'd':
			if(optarg != NULL)
				zwdevice = optarg;
			break;
		case 'l':
			if(optarg != NULL)
				logLevel=atoi(optarg);
				if (logLevel < 1)
					logLevel = 1;
				if (logLevel > 5)
					logLevel = 5;
			break;
		case 'n':
			if(optarg != NULL)
				nodeID=atoi(optarg);
			break;
		case 's':
			if(optarg != NULL)
				nodeLevel=atoi(optarg);
			break;
		default:
			printf("\nUsage:\n");
			printf("zwave <options>\n");
			printf("  -d [device]				Device.  Default device: /dev/ttyUSB0\n");
			printf("  -k 					Keep alive.  Polls devices for status every 30 seconds\n");
			printf("  -l [log level]			Set logging level (1-5)\n");
			printf("					     (1-ERROR, 2-WARN, 3-INFO, 4-ALL (Default), 5-CONSOLE)\n");
			printf("  -n [node ID]				Node ID\n");
			printf("  -p  					Probe Z-Wave devices (work in progress)\n");
			printf("  -s [node level]			Set level (0-255)\n");
			exit (0);
			break;
		}

	}

	logInit("//var//log//zwave.log");
	initZWave(zwdevice);

	if (v_initflag)
		initZWNetwork();

  if (nodeID > 1)
    commandBasicSet(nodeID,nodeLevel, 0);

	if (v_initflag)
		sleep(20);

	if (v_keepalive)
		while(1);

	while (zwQueueIndex > 0)
		usleep(500000);

	logClose();
	return 0;
}
