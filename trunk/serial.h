/************************************************************************************
*
*		serial.h
*
*		Implementation of a logging function
*
*************************************************************************************/

#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <sys/time.h>
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/file.h>


#define ERR_OPEN 			1
#define ERR_TIMEOUT		4
#define ERR_FLOCK			22
#define ERR_STTY				23


int OpenSerialPortEx (const char Pname[],int *port);
int ReadSerialStringEx (int dev,char pnt[],int len,long timeout);
int WriteSerialStringEx (int dev,char pnt[],int len);

#endif