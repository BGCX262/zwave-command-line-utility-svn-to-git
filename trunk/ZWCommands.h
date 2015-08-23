/************************************************************************************
*
*		ZWCommands.h
*
*		Z-Wave utility.  Z-Wave command frame builder.
*
*************************************************************************************/

#ifndef _ZWCOMMANDS_H_
#define _ZWCOMMANDS_H_

void commandBasicSet(int nodeID,int nodeLevel, int instance);
void commandMultilevelSet (int nodeID, int nodeLevel, int duration, int instance);
void zwSoftReset(void);
void zwRequestBasicReport(int node_id, int instance);
void zwRequestMultilevelSensorReport(int node_id);
void zwSendBasicReport(int node_id);
void zwRequestNodeInfo(int node_id);
void zwRequestNodeProtocolInfo(int node_id);

#endif
