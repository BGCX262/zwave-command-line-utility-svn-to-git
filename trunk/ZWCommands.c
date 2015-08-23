/************************************************************************************
*
*		ZWCommands.c
*
*		Z-Wave utility.  Z-Wave command frame builder.
*
*************************************************************************************/

#include "main.h"
#include "logging.h"
#include "ZWApi.h"
#include "ZWCommands.h"
#include "serial.h"

char sendbuf[512];	 //Serial port send buffer

void commandBasicSet (int nodeID, int nodeLevel, int instance)
{

// Set value on ZWave node

	if (instance == 0) {
		sendbuf[0] = FUNC_ID_ZW_SEND_DATA;
		sendbuf[1] = nodeID;
		sendbuf[2] = 3;
		sendbuf[3] = COMMAND_CLASS_BASIC;
		sendbuf[4] = BASIC_SET;
		sendbuf[5] = nodeLevel;
		sendbuf[6] = TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
		sendFunction(sendbuf , 7, REQUEST, 1);
	} else {
		sendbuf[0] = FUNC_ID_ZW_SEND_DATA;
		sendbuf[1] = nodeID;
		sendbuf[2] = 6;
		sendbuf[3] = COMMAND_CLASS_MULTI_INSTANCE;
		sendbuf[4] = MULTI_INSTANCE_CMD_ENCAP;
		sendbuf[5] = instance;
		sendbuf[6] = COMMAND_CLASS_BASIC;
		sendbuf[7] = BASIC_SET;
		sendbuf[8] = nodeLevel;
		sendbuf[9] = TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
		sendFunction(sendbuf , 10, REQUEST, 1);
	}
}


void commandMultilevelSet (int nodeID, int nodeLevel, int duration, int instance)
{

// Set value on ZWave node

	if (instance == 0) {
		sendbuf[0] = FUNC_ID_ZW_SEND_DATA;
		sendbuf[1] = nodeID;
		sendbuf[2] = 3;
		sendbuf[3] = COMMAND_CLASS_SWITCH_MULTILEVEL;
		sendbuf[4] = SWITCH_MULTILEVEL_SET;
		sendbuf[5] = nodeLevel;
		sendbuf[6] = duration;
		sendbuf[7] = TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
		sendFunction(sendbuf , 8, REQUEST, 1);
	} else {
		sendbuf[0] = FUNC_ID_ZW_SEND_DATA;
		sendbuf[1] = nodeID;
		sendbuf[2] = 6;
		sendbuf[3] = COMMAND_CLASS_MULTI_INSTANCE;
		sendbuf[4] = MULTI_INSTANCE_CMD_ENCAP;
		sendbuf[5] = instance;
		sendbuf[6] = COMMAND_CLASS_SWITCH_MULTILEVEL;
		sendbuf[7] = SWITCH_MULTILEVEL_SET;
		sendbuf[8] = nodeLevel;
		sendbuf[9] = duration;
		sendbuf[10] = TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
		sendFunction(sendbuf , 11, REQUEST, 1);
	}
}


void zwSoftReset(void)
{

	logInfo(LOG_INFO, "Soft-resetting the Z-Wave chip");
	sendbuf[0] = FUNC_ID_SERIAL_API_SOFT_RESET;
	sendFunction( sendbuf , 1, REQUEST, 0);
}


void zwRequestBasicReport(int node_id, int instance)
{

	if (instance == 0) {
		sendbuf[0] = FUNC_ID_ZW_SEND_DATA;
		sendbuf[1] = node_id;
		sendbuf[2] = 0x02;
		sendbuf[3] = COMMAND_CLASS_BASIC;
		sendbuf[4] = BASIC_GET;
		sendbuf[5] = TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
		sendFunction (sendbuf,6,REQUEST,1);
	} else {

		sendbuf[0] = FUNC_ID_ZW_SEND_DATA;
		sendbuf[1] = node_id;
		sendbuf[2] = 5;
		sendbuf[3] = COMMAND_CLASS_MULTI_INSTANCE;
		sendbuf[4] = MULTI_INSTANCE_CMD_ENCAP;
		sendbuf[5] = instance;
		sendbuf[6] = COMMAND_CLASS_BASIC;
		sendbuf[7] = BASIC_GET;
		sendbuf[8] = TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
		sendFunction( sendbuf , 9, REQUEST, 1);
	}
}


void zwRequestMultilevelSensorReport(int node_id)
{

	sendbuf[0] = FUNC_ID_ZW_SEND_DATA;
	sendbuf[1] = node_id;
	sendbuf[2] = 2; // length of command
	sendbuf[3] = COMMAND_CLASS_SENSOR_MULTILEVEL;
	sendbuf[4] = SENSOR_MULTILEVEL_GET;
	sendbuf[5] = TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
	sendFunction( sendbuf , 6, REQUEST, 1);
}


void zwSendBasicReport(int node_id)
{

	sendbuf[0] = FUNC_ID_ZW_SEND_DATA;
	sendbuf[1] = node_id;
	sendbuf[2] = 3;
	sendbuf[3] = COMMAND_CLASS_BASIC;
	sendbuf[4] = BASIC_REPORT;
	sendbuf[5] = 0x00;
	sendbuf[6] = TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
	sendFunction( sendbuf , 7, REQUEST, 1);
}


void zwRequestNodeInfo(int node_id)
{
	sendbuf[0] = FUNC_ID_ZW_REQUEST_NODE_INFO;
	sendbuf[1] = node_id;
	sendFunction( sendbuf , 2, REQUEST, 0);
}


void zwRequestNodeProtocolInfo(int node_id)
{
	sendbuf[0]=FUNC_ID_ZW_GET_NODE_PROTOCOL_INFO;
	sendbuf[1]=node_id;
	sendFunction( sendbuf , 2, REQUEST, 0);
}
