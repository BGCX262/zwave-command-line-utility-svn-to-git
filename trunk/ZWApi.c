/************************************************************************************
*
*		ZWApi.c
*
*		Z-Wave utility.  Z-Wave API.
*
*************************************************************************************/


#include "ZWApi.h"
#include "logging.h"
#include "serial.h"
#include "ZWCommands.h"

char sendbuf[256];	 //Serial port send buffer
int h_controller;  //Controller port handle
char logstring[256];  //String for logging function
unsigned char callbackPool;  //Callback ID

int poll_state;

int await_ack;  //Flag to signal that an ACK is expected
char await_callback;  //Callback ID we're currently waiting for
char callback_type; //Command associated with current callback ID
int retry_count;  //Number of times the current command has been resent
char callbackid;
int controllernode;  //Node ID for controller

// controller dump
int memory_dump_offset;
int memory_dump_len;
int memory_dump_counter;

unsigned char memory_dump[16384];


pthread_mutex_t mutexSendQueue; //Mutex to lock command queue;
static pthread_t readThread; //Thread handle for serial reader;
int threadResult;  //Result code for thread creation

typedef struct ZWJob {
	char buffer[256];
	int len;
	int sendcount;
	int callbackid;
	int callback_type;
	int await_response;
} ZWJob;		//ZWave command

ZWJob zwCommandQueue[16];	//Queued commands

typedef struct ZWIntent {
	int type;
	int nodeid;
	time_t timeout;
	int retrycount;
} ZWIntent;

ZWIntent zwIntentQueue[232];	//Queue for node intents

typedef struct ZWNode {
	int id;
	int iPKDevice;
	int typeBasic;
	int typeGeneric;
	int typeSpecific;
	int sleepingDevice;
	int secureDevice;
	int nonceCounter;
	unsigned char nonce[8];
	int stateBasic;
	char associationList[4][32];
} ZWNode;

ZWNode  zwNodes[32];  //Discovered nodes

int	zwQueueIndex;	//Command queue index
int	zwIntentIndex;	//Intent index for device discovery
int	zwNodeIndex;	//Index for node discovery

int 	maxnodeid;

void initZWave(char *zwdevice)
{
// Open Z-Wave controller port
	poll_state = 1;
	zwNodeIndex=0;
	callbackPool = 0x61;

	OpenSerialPortEx(zwdevice, &h_controller);
	if(h_controller < 0) {
		logInfo(LOG_ERROR, "Failed to open Z-Wave serial device\n");
		exit(EXIT_FAILURE);
	}
	sendbuf[0] = NAK;
	WriteSerialStringEx(h_controller,sendbuf,1);

	zwQueueIndex= 0;
	zwIntentIndex = 0;
	maxnodeid = 0;

	pthread_mutexattr_t mutexAttrRecursive;
	pthread_mutexattr_init(&mutexAttrRecursive);
	pthread_mutexattr_settype(&mutexAttrRecursive, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mutexSendQueue, &mutexAttrRecursive);

	threadResult = pthread_create(&readThread, NULL, (void *)receiveFunction, NULL);
	if (threadResult !=0) {
		logInfo(LOG_ERROR,"Failed to start receive thread.  Exiting. \n");
		exit(EXIT_FAILURE);
	}

	logInfo(LOG_INFO,"Receive thread started\n");
}


void initZWNetwork(void)
{

	sendbuf[0] = ZW_GET_VERSION;
	logInfo(LOG_INFO,"Sending: Get version\n");
	sendFunction(sendbuf, 1, REQUEST,0);

	sendbuf[0] = ZW_MEMORY_GET_ID;
	logInfo(LOG_INFO,"Sending: Get home/node id\n");
	sendFunction(sendbuf, 1, REQUEST,0);

	sendbuf[0] =  FUNC_ID_SERIAL_API_GET_CAPABILITIES;
	logInfo(LOG_INFO,"Sending: Get capabilities\n");
	sendFunction(sendbuf, 1, REQUEST,0);

	sendbuf[0] =  FUNC_ID_ZW_GET_SUC_NODE_ID;
	logInfo(LOG_INFO,"Sending: Get SUC node id\n");
	sendFunction(sendbuf, 1, REQUEST,0);

	sendbuf[0] = FUNC_ID_SERIAL_API_GET_INIT_DATA;
	logInfo(LOG_INFO,"Sending: Get init data\n");
	sendFunction(sendbuf, 1, REQUEST,0);

}


void *decodeFrame(char *frame, int length)
{
	int i,j;
	char byte[256];
	int tmp_nodeid;
	char tempbyte[16];
	char routingtable[512];
	unsigned char nodebit;

	if (frame[2] == RESPONSE) {
		logInfo(LOG_ALL,"Decoding RESPONSE frame...\n");
		switch((unsigned char)frame[3]) {

		case FUNC_ID_ZW_SEND_DATA:
			switch((unsigned char)frame[4]) {
			case 1:
				logInfo(LOG_ALL,"ZW_SEND delivered to Z-Wave stack\n");
				break;

			case 0:
				logInfo(LOG_WARN,"ERROR: ZW_SEND could not be delivered to Z-Wave stack\n");
				break;

			default:
				logInfo(LOG_WARN,"ERROR: Received ZW_SEND RESPONSE frame is invalid!\n");
				break;
			}
			break;

		case ZW_GET_ROUTING_INFO:
			logInfo(LOG_INFO,"Got reply to ZWGET_ROUTING_INFO\n");
			strcpy(routingtable,"");
			j = getIntent(2);
			sprintf(tempbyte,"Node %3i: ",j);
			strcat(routingtable, tempbyte);
			for (i=1; i <=maxnodeid; i++) {
				nodebit = (unsigned char)frame[4+i/8] & (1<<((i%8)-1));
				if (nodebit)
					strcat(routingtable,"Y");
				else
					strcat(routingtable,"N");
			}
			strcat(routingtable,"\n");
			if (j == maxnodeid)
				logInfo(LOG_INFO,routingtable);
			break;

		case ZW_GET_VERSION:
			sprintf(logstring ,"Got reply to ZW_VERSION: ");
			for(i=4; i <= frame[1]-1; i++)
				logstring[21+i] = frame[i];
			strcat(logstring, "\n");
			logInfo(LOG_INFO, logstring);
			break;

		case ZW_MEMORY_GET_ID:
			sprintf(logstring ,"Got reply to ZW_MEMORY_GET_ID: Home ID is 0x%02X%02X%02X%02X%02X\n", (unsigned char)frame[4],(unsigned char)frame[5],(unsigned char)frame[6],(unsigned char)frame[7],(unsigned char)frame[8]);
			logInfo(LOG_INFO, logstring);
			controllernode = (unsigned char)frame[8];
			break;

		case FUNC_ID_SERIAL_API_GET_CAPABILITIES:
			sprintf(logstring,"Got reply to FUNC_ID_SERIAL_API_GET_CAPABILITIES: ");
			sprintf(byte,"Serial API version: %i.%i, Manufacturer: %i, Product Type: %i, Product ID: %i\n",(unsigned char)frame[4],(unsigned char)frame[5], ((unsigned char)frame[6]<<8) + (unsigned char)frame[7],((unsigned char)frame[8]<<8) + (unsigned char)frame[9],((unsigned char)frame[10]<<8) + (unsigned char)frame[11]);
			strcat(logstring, byte);
			logInfo(LOG_INFO, logstring);
			break;

		case ZW_MEM_GET_BUFFER:
			logInfo(LOG_INFO,"Got reply to ZW_MEM_GET_BUFFER");
			for (i=2; i<length; i++) {
				memory_dump[memory_dump_offset+i-2]=(unsigned char)frame[i];
				memory_dump_counter++;
			}
			if ((memory_dump_offset > -1) && (memory_dump_offset < 16320)) {
				// we are dumping the complete memory
				memory_dump_offset+=60;
				zwReadMemory(memory_dump_offset,60);
			} else if ((memory_dump_offset == 16320)) {
				// last frame
				memory_dump_offset+=60;
				logInfo(LOG_INFO,"Memory dump fetch last 4 bytes\n");
				zwReadMemory(memory_dump_offset,4);
			} else if (memory_dump_offset > 16320) {
				int fd, writesize;
				logInfo(LOG_INFO,"Memory dump complete\n");
				for (i=0; i<16384; i++) {
					if (i%32==0) {
						printf("\n");
					}
					printf("%2x ",memory_dump[i]);
				}
				printf("\n");
				fd = open(".//zwave-controller-backup.dump",O_WRONLY|O_CREAT);
				writesize = write(fd, memory_dump, 16384);
				close(fd);
				sprintf(logstring,"Dumped %i bytes, offset %i. written %i bytes\n",memory_dump_counter, memory_dump_offset, writesize);
				logInfo(LOG_INFO,logstring);
				poll_state=1;
			}
			break;

		case ZW_MEM_PUT_BUFFER:
			logInfo(LOG_INFO,"Got reply to ZW_MEM_PUT_BUFFER\n");
			if (memory_dump_offset < 16380) {
				memory_dump_offset+=60;
				zwWriteMemory(memory_dump_offset, memory_dump_len, memory_dump+memory_dump_offset);
			} else if (memory_dump_offset == 16380) {
				memory_dump_len=4;
				zwWriteMemory(memory_dump_offset, memory_dump_len, memory_dump+memory_dump_offset);
				memory_dump_offset+=4;
			} else {
				sprintf(logstring,"Memory restore complete %i",memory_dump_offset);
				logInfo(LOG_INFO,logstring);
				memory_dump_offset=65535;
			}
			break;

		case FUNC_ID_ZW_GET_SUC_NODE_ID:
			sprintf(logstring,"Got reply to GET_SUC_NODE_ID, node: %i\n",frame[2]);
			logInfo(LOG_INFO, logstring);

			if ((unsigned char)frame[4] == 0) {
				logInfo(LOG_WARN,"No SUC, we become SUC\n");
				sendbuf[0]=FUNC_ID_ZW_ENABLE_SUC;
				sendbuf[1]=1; // 0=SUC,1=SIS
				sendbuf[2]=ZW_SUC_FUNC_NODEID_SERVER;
				sendFunction( sendbuf , 3, REQUEST, 0);

				sendbuf[0]=FUNC_ID_ZW_SET_SUC_NODE_ID;
				sendbuf[1]=controllernode;
				sendbuf[2]=1; // TRUE, we want to be SUC/SIS
				sendbuf[3]=0; // no low power
				sendbuf[4]=ZW_SUC_FUNC_NODEID_SERVER;
				sendFunction( sendbuf , 5, REQUEST, 1);

			} else if ((unsigned char)frame[4] != controllernode) {
				sprintf(logstring,"Controller mismatch: %i vs. %i.  Requesting network update from SUC.\n", frame[4],controllernode);
				logInfo(LOG_WARN,logstring);
				sendbuf[0]=FUNC_ID_ZW_REQUEST_NETWORK_UPDATE;
				sendFunction(sendbuf, 1, REQUEST, 1);
			}
			break;

		case FUNC_ID_SERIAL_API_GET_INIT_DATA:
			logInfo(LOG_INFO,"Got reply to FUNC_ID_SERIAL_API_GET_INIT_DATA:\n");
			if (frame[6] == MAGIC_LEN) {
				for (i=7; i<7+MAGIC_LEN; i++) {
					for (j=0; j<8; j++) {
						if ((unsigned char)frame[i] & (0x01 << j)) {
							tmp_nodeid = (i-7)*8+j+1;
							sprintf(logstring,"Found node %d.  Requesting protocol capabilities.\n",tmp_nodeid);
							logInfo(LOG_INFO,logstring);
							addIntent(tmp_nodeid,1);

							zwRequestNodeProtocolInfo(tmp_nodeid);
							sprintf(logstring,"Requesting command class data from node %i\n",tmp_nodeid);
							logInfo(LOG_INFO,logstring);
							zwRequestNodeInfo(tmp_nodeid);
						} else {
							sprintf(logstring,"Node id %i not in node mask\n",(i-7)*8+j+1);
							logInfo(LOG_ALL,logstring);
						}
					}
				}
			} else {
				logInfo(LOG_ERROR,"Node information frame format error\n");
			}
			break;

		case FUNC_ID_ZW_GET_NODE_PROTOCOL_INFO:
			logInfo(LOG_INFO,"Got reply to FUNC_ID_ZW_GET_NODE_PROTOCOL_INFO\n");
			/*			tmp_nodeid = getIntent(1);

						// test if node is valid
						if (frame[8] != 0) {
							sprintf(logstring,"***FOUND NODE: %d\n",tmp_nodeid);
							logInfo(LOG_INFO,logstring);
							if (tmp_nodeid > maxnodeid)
								maxnodeid = tmp_nodeid;

							zwNodes[zwNodeIndex].typeBasic = (unsigned char) frame[7];
							zwNodes[zwNodeIndex].typeGeneric = (unsigned char) frame[8];
							zwNodes[zwNodeIndex].typeSpecific = (unsigned char) frame[9];
							zwNodes[zwNodeIndex].stateBasic = -1;
							strcpy(zwNodes[zwNodeIndex].associationList[0],"");
							strcpy(zwNodes[zwNodeIndex].associationList[1],"");
							strcpy(zwNodes[zwNodeIndex].associationList[2],"");
							strcpy(zwNodes[zwNodeIndex].associationList[3],"");

							if (((unsigned char)frame[4]) & (0x01 << 7)) {
								logInfo(LOG_INFO,"\tListening node\n");
								zwNodes[zwNodeIndex].sleepingDevice = FALSE;
							} else {
								logInfo(LOG_INFO,"\tSleeping node\n");
								zwNodes[zwNodeIndex].sleepingDevice = TRUE;
							}
							if (((unsigned char)frame[5]) & (0x01 << 7)) {
								logInfo(LOG_INFO,"\tOptional functionality\n");
							}
							if (((unsigned char)frame[5]) & (0x01 << 6)) {
								logInfo(LOG_INFO,"\t1000ms frequent listening slave\n");
								zwNodes[zwNodeIndex].sleepingDevice = FALSE;
							}
							if (((unsigned char)frame[5]) & (0x01 << 5)) {
								logInfo(LOG_INFO,"\t250ms frequent listening slave\n");
								zwNodes[zwNodeIndex].sleepingDevice = FALSE;
							}

							switch (frame[7]) {
							case BASIC_TYPE_CONTROLLER:
								logInfo(LOG_INFO,"\tBASIC TYPE: Controller\n");
								break;

							case BASIC_TYPE_STATIC_CONTROLLER:
								logInfo(LOG_INFO,"\tBASIC TYPE: Static Controller\n");
								break;

							case BASIC_TYPE_SLAVE:
								logInfo(LOG_INFO,"\tBASIC TYPE: Slave\n");
								break;

							case BASIC_TYPE_ROUTING_SLAVE:
								logInfo(LOG_INFO,"\tBASIC TYPE: Routing Slave\n");
								break;

							default:
								sprintf(logstring,"BASIC TYPE: %x",frame[7]);
								logInfo(LOG_INFO,logstring);
								break;
							}

							switch ((unsigned char)frame[8]) {
							case GENERIC_TYPE_GENERIC_CONTROLLER:
								logInfo(LOG_INFO,"\tGENERIC TYPE: Generic Controller\n");
								break;

							case GENERIC_TYPE_STATIC_CONTROLLER:
								logInfo(LOG_INFO,"\tGENERIC TYPE: Static Controller\n");
								break;

							case GENERIC_TYPE_THERMOSTAT:
								logInfo(LOG_INFO,"\tGENERIC TYPE: Thermostat\n");
								break;

							case GENERIC_TYPE_SWITCH_MULTILEVEL:
								logInfo(LOG_INFO,"\tGENERIC TYPE: Multilevel Switch\n");
								break;

							case GENERIC_TYPE_SWITCH_REMOTE:
								logInfo(LOG_INFO,"\tGENERIC TYPE: Remote Switch\n");
								// TODO: saner approach
								//  we read the associations from groups one and two (left and right paddle on the ACT remote switch)
								//zwAssociationGet(tmp_nodeid,1);
								//zwAssociationGet(tmp_nodeid,2);
								break;

							case GENERIC_TYPE_SWITCH_BINARY:
								logInfo(LOG_INFO,"\tGENERIC TYPE: Binary Switch\n");
								break;

							case GENERIC_TYPE_SENSOR_BINARY:
								logInfo(LOG_INFO,"\tGENERIC TYPE: Sensor Binary\n");
								break;
							case GENERIC_TYPE_WINDOW_COVERING:
								logInfo(LOG_INFO,"\tGENERIC TYPE: Window Covering\n");
								break;

							case GENERIC_TYPE_SENSOR_MULTILEVEL:
								logInfo(LOG_INFO,"\tGENERIC TYPE: Sensor Multilevel\n");
								break;

							case GENERIC_TYPE_SENSOR_ALARM:
								logInfo(LOG_INFO,"\tGENERIC TYPE: Sensor Alarm\n");
								break;

							default:
								sprintf(logstring,"\tGENERIC TYPE: 0x%x\n",frame[8]);
								logInfo(LOG_INFO,logstring);
								break;

							}

							sprintf(logstring,"\tSPECIFIC TYPE: 0x%x\n",frame[9]);
							logInfo(LOG_INFO,logstring);

							// check if we have fetched the node's command classes/capabilities
							string capa = DCEcallback->GetCapabilities(tmp_nodeid, 0);
							if (!capa.empty()) {
								logInfo(LOG_INFO,"Device capabilities: %s",capa.c_str());
								resetNodeInstanceCount(newNode, capa);
								// Is this a MULTI_INSTANCE node?
								if (capa.find(StringUtils::itos(COMMAND_CLASS_MULTI_INSTANCE)) != string::npos) {
									// load capabilities for instances and store in node
									for ( ccIterator=newNode->mapCCInstanceCount.begin() ; ccIterator != newNode->mapCCInstanceCount.end(); ccIterator++ ) {
										logInfo(LOG_INFO,"Command class: %d (hex: %x)", (*ccIterator).first, (*ccIterator).first);
										if (mapCCInstanceCount.find((*ccIterator).first) != mapCCInstanceCount.end()) {
											logInfo(LOG_INFO,"Device data matches: instance count: %d", mapCCInstanceCount[(*ccIterator).first]);
											newNode->mapCCInstanceCount[(*ccIterator).first] = mapCCInstanceCount[(*ccIterator).first];
										}
									}
								}
							} else {
								// No capabilities on record, request node info
								logInfo(LOG_INFO,"\tNo device capabilities for node: %d\n",newNode->id);
								zwRequestNodeInfo(newNode->id);
							}
						} else {
							sprintf(logstring, "\tInvalid generic class (0x%X), ignoring device\n",(unsigned char)frame[8]);
							logInfo(LOG_WARN, logstring);
						}

						if (zwIntentIndex == 0) {
							// we got all protcol info responses
							logInfo(LOG_INFO,"Finished building node list\n\n");
						}
						} */
			break;

		case FUNC_ID_ZW_REQUEST_NODE_INFO:
			logInfo(LOG_INFO,"Got reply to FUNC_ID_ZW_REQUEST_NODE_INFO\n");

			sprintf(logstring, "Received %i NODE_INFO bytes.  Frame: ", length);
			for(i=0; i < length; i++) {
				sprintf(tempbyte,"0x%02X ",(unsigned char)frame[i]);
				strncat(logstring,tempbyte,5);

			}
			strcat(logstring, "\n");
			logInfo(LOG_ALL,logstring);

			break;

		default:
			sprintf(logstring,"Unhandled RESPONSE frame: 0x%02X\n", frame[3]);
			logInfo(LOG_WARN,logstring);
			break;
		}

	} else if (frame[2] == REQUEST) {
		logInfo(LOG_ALL,"Decoding REQUEST frame...\n");
		switch((unsigned char)frame[3]) {
		case FUNC_ID_ZW_SEND_DATA:
			sprintf(logstring,"ZW_SEND Response with callback 0x%02X received\n",(unsigned char)frame[4]);
			logInfo(LOG_ALL,logstring);
			if ((unsigned char)frame[4] != await_callback) {
				// wrong callback id
				logInfo(LOG_INFO,"ERROR: callback id is invalid!");
			} else {
				switch((unsigned char)frame[5]) {
				case 1:
					pthread_mutex_lock (&mutexSendQueue);

					if (zwCommandQueue[0].sendcount > 2) {
						// can't deliver frame, abort
						sprintf(logstring,"ERROR: ZW_SEND command failed for callback ID 0x%02X.  Removing job after 3 retries.\n", (unsigned char)frame[4]);
						logInfo(LOG_INFO,logstring);
						zwPopJob();
					} else {
						sprintf(logstring,"ERROR: ZW_SEND command failed for callback ID 0x%02X.  Retrying.\n", (unsigned char)frame[4]);
						logInfo(LOG_ERROR,logstring);
						// trigger resend
						await_ack = 0;
					}
					await_callback = 0;
					pthread_mutex_unlock (&mutexSendQueue);
					break;

				case 0:
					sprintf(logstring,"ZW_SEND command successful for callback ID 0x%02X\n", (unsigned char)frame[4]);
					logInfo(LOG_ALL,logstring);
					await_callback = 0;
					pthread_mutex_lock (&mutexSendQueue);
					zwPopJob();
					pthread_mutex_unlock (&mutexSendQueue);
					break;

				default:
					sprintf(logstring,"ERROR: Received ZW_SEND REQUEST frame is invalid for callback ID 0x%02X\n",(unsigned char)frame[4]);
					logInfo(LOG_WARN,logstring);
					break;
				}
			}
			break;

		case FUNC_ID_ZW_ADD_NODE_TO_NETWORK:
			logInfo(LOG_WARN,"FUNC_ID_ZW_ADD_NODE_TO_NETWORK: Not implemented");
			switch (frame[5]) {
			case ADD_NODE_STATUS_LEARN_READY:
				logInfo(LOG_WARN,"ADD_NODE_STATUS_LEARN_READY\n");
				break;
			case ADD_NODE_STATUS_NODE_FOUND:
				logInfo(LOG_WARN,"ADD_NODE_STATUS_NODE_FOUND\n");
				break;
			case ADD_NODE_STATUS_ADDING_SLAVE:
				logInfo(LOG_WARN,"ADD_NODE_STATUS_ADDING_SLAVE\n");
				break;
			case ADD_NODE_STATUS_ADDING_CONTROLLER:
				logInfo(LOG_WARN,"ADD_NODE_STATUS_ADDING_CONTROLLER\n");
				break;
			case ADD_NODE_STATUS_PROTOCOL_DONE:
				logInfo(LOG_WARN,"ADD_NODE_STATUS_PROTOCOL_DONE\n");
				break;
			case ADD_NODE_STATUS_DONE:
				logInfo(LOG_WARN,"ADD_NODE_STATUS_DONE\n");
				break;
			case ADD_NODE_STATUS_FAILED:
				logInfo(LOG_WARN,"ADD_NODE_STATUS_FAILED\n");
				break;
			default:
				break;
			}
			break;

		case FUNC_ID_ZW_REMOVE_NODE_FROM_NETWORK:
			logInfo(LOG_WARN,"FUNC_ID_ZW_REMOVE_NODE_FROM_NETWORK: Not implemented");
			switch (frame[5]) {
			case REMOVE_NODE_STATUS_LEARN_READY:
				logInfo(LOG_WARN,"REMOVE_NODE_STATUS_LEARN_READY\n");
				break;
			case REMOVE_NODE_STATUS_NODE_FOUND:
				logInfo(LOG_WARN,"REMOVE_NODE_STATUS_NODE_FOUND\n");
				break;
			case REMOVE_NODE_STATUS_ADDING_SLAVE:
				logInfo(LOG_WARN,"REMOVE_NODE_STATUS_ADDING_SLAVE\n");
				break;
			case REMOVE_NODE_STATUS_ADDING_CONTROLLER:
				logInfo(LOG_WARN,"REMOVE_NODE_STATUS_ADDING_CONTROLLER\n");
				break;
			case REMOVE_NODE_STATUS_PROTOCOL_DONE:
				logInfo(LOG_WARN,"REMOVE_NODE_STATUS_PROTOCOL_DONE\n");
				break;
			case REMOVE_NODE_STATUS_DONE:
				logInfo(LOG_WARN,"REMOVE_NODE_STATUS_DONE\n");
				break;
			case REMOVE_NODE_STATUS_FAILED:
				logInfo(LOG_WARN,"REMOVE_NODE_STATUS_FAILED\n");
				break;
			default:
				break;
			}
			break;

		case FUNC_ID_ZW_APPLICATION_UPDATE:
			switch((unsigned char)frame[4]) {

			case UPDATE_STATE_NODE_INFO_RECEIVED:
				sprintf(logstring,"FUNC_ID_ZW_APPLICATION_UPDATE:UPDATE_STATE_NODE_INFO_RECEIVED received from node %d - ",(unsigned int)frame[5]);
				switch((unsigned char)frame[7]) {
				case BASIC_TYPE_ROUTING_SLAVE:
				case BASIC_TYPE_SLAVE:
					logInfo(LOG_INFO,"BASIC_TYPE_SLAVE:\n");
					switch(frame[8]) {
					case GENERIC_TYPE_SWITCH_MULTILEVEL:
						strcat(logstring,"GENERIC_TYPE_SWITCH_MULTILEVEL\n");
						sendbuf[0] = FUNC_ID_ZW_SEND_DATA;
						sendbuf[1] = frame[5];
						sendbuf[2] = 0x02;
						sendbuf[3] = COMMAND_CLASS_SWITCH_MULTILEVEL;
						sendbuf[4] = SWITCH_MULTILEVEL_GET;
						sendbuf[5] = TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
						sendFunction (sendbuf,6,REQUEST,1);
						sendbuf[0] = FUNC_ID_ZW_SEND_DATA;
						sendbuf[1] = frame[5];
						sendbuf[2] = 0x02;
						sendbuf[3] = COMMAND_CLASS_BASIC;
						sendbuf[4] = BASIC_GET;
						sendbuf[5] = TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
						sendFunction (sendbuf,6,REQUEST,1);
						break;

					case GENERIC_TYPE_SWITCH_BINARY:
						strcat(logstring,"GENERIC_TYPE_SWITCH_BINARY\n");
						sendbuf[0] = FUNC_ID_ZW_SEND_DATA;
						sendbuf[1] = frame[5];
						sendbuf[2] = 0x02;
						sendbuf[3] = COMMAND_CLASS_BASIC;
						sendbuf[4] = BASIC_GET;
						sendbuf[5] = TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
						sendFunction (sendbuf,6,REQUEST,1);
						break;

					case GENERIC_TYPE_SWITCH_REMOTE:
						strcat(logstring,"GENERIC_TYPE_SWITCH_REMOTE\n");
						break;

					case GENERIC_TYPE_SENSOR_MULTILEVEL:
						strcat(logstring,"GENERIC_TYPE_SENSOR_MULTILEVEL\n");
						break;

					default:
						strcat(logstring,"unhandled class\n");
						break;
					}
					logInfo(LOG_INFO,logstring);
					break;
				}

			case UPDATE_STATE_NODE_INFO_REQ_FAILED:
				logInfo(LOG_INFO,"FUNC_ID_ZW_APPLICATION_UPDATE:UPDATE_STATE_NODE_INFO_REQ_FAILED received\n");
				break;

			case UPDATE_STATE_NEW_ID_ASSIGNED:
				sprintf(logstring,"** Network change **: ID %d was assigned to a new Z-Wave node\n",(unsigned char)frame[3]);
				logInfo(LOG_WARN,logstring);
				break;

			case UPDATE_STATE_DELETE_DONE:
				sprintf(logstring,"** Network change **: Z-Wave node %d was removed\n",(unsigned char)frame[3]);
				logInfo(LOG_WARN,logstring);
				break;

			default:
				break;
			}
			break;

		case FUNC_ID_APPLICATION_COMMAND_HANDLER:
			logInfo(LOG_INFO,"FUNC_ID_APPLICATION_COMMAND_HANDLER:\n");
			switch ((unsigned char)frame[7]) {
			case COMMAND_CLASS_CONTROLLER_REPLICATION:
				logInfo(LOG_INFO,"COMMAND_CLASS_CONTROLLER_REPLICATION\n");
				if (frame[8] == CTRL_REPLICATION_TRANSFER_GROUP) {
					// we simply ack the group information for now
					sendbuf[0]=FUNC_ID_ZW_REPLICATION_COMMAND_COMPLETE;
					sendFunction( sendbuf , 1, REQUEST, 0);
				} else {
					// ack everything else, too
					sendbuf[0]=FUNC_ID_ZW_REPLICATION_COMMAND_COMPLETE;
					sendFunction( sendbuf , 1, REQUEST, 0);
				}
				break;

			case COMMAND_CLASS_MULTI_INSTANCE:
				logInfo(LOG_INFO,"COMMAND_CLASS_MULTI_INSTANCE");
				/*						if (frame[6] == MULTI_INSTANCE_REPORT) {
										        int instanceCount = (unsigned char)frame[8];
											DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"Got MULTI_INSTANCE_REPORT from node %i: Command Class 0x%x, instance count: %i",(unsigned char)frame[3],(unsigned char)frame[7], instanceCount);
											// instance count == 1 -> assume instance 1 is "main" device and don't add new device
											if (instanceCount > 1) {
											        ZWNodeMapIt = ZWNodeMap.find((unsigned int)frame[3]);
												if (ZWNodeMapIt != ZWNodeMap.end()) {
												        (*ZWNodeMapIt).second->mapCCInstanceCount[(unsigned int)frame[7]] = instanceCount;
												}
												// add new devices for instances
												char tmpstr[30];
												sprintf(tmpstr, "%d", (unsigned char)frame[3]);
												for (int i = 2; i <= instanceCount; i++) {
												        int PKDevice = DCEcallback->AddDevice(0, tmpstr, i, (*ZWNodeMapIt).second->plutoDeviceTemplateConst);
													DCEcallback->AddCapability(PKDevice, (unsigned char)frame[7]);
													newNode->iPKDevice = PKDevice;
												}
											}
										} else if (frame[6] == MULTI_INSTANCE_CMD_ENCAP) {
										        DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"Got MULTI_INSTANCE_CMD_ENCAP from node %i: instance %i Command Class 0x%x type 0x%x",(unsigned char)frame[3],(unsigned char)frame[7],(unsigned char)frame[8],(unsigned char)frame[9]);
										        if (frame[8] == COMMAND_CLASS_SENSOR_MULTILEVEL) {
											        handleCommandSensorMultilevelReport(frame[3], //nodeid
																    (unsigned int)frame[7], // instance id
																    (unsigned int)frame[10], // sensor type
																    (unsigned int)frame[11], // value metadata
																    (unsigned int)frame[12], // value
																    (unsigned int)frame[13],
																    (unsigned int)frame[14],
																    (unsigned int)frame[15]);
										        } else if ((frame[8] == COMMAND_CLASS_BASIC) && (frame[9] == BASIC_REPORT)) {
												// 41	07/22/10 12:05:17.485		0x1 0xc 0x0 0x4 0x0 0x7 0x6 0x60 0x6 0x1 0x20 0x3 0x0 0xb2 (#######`## ###) <0xb795fb90>
												// 36	07/22/10 12:05:17.485		FUNC_ID_APPLICATION_COMMAND_HANDLER: <0xb795fb90>
												// 36	07/22/10 12:05:17.485		COMMAND_CLASS_MULTI_INSTANCE <0xb795fb90>
												// 36	07/22/10 12:05:17.485		Got MULTI_INSTANCE_CMD_ENCAP from node 7: instance 1 Command Class 0x20 type 0x3 <0xb795fb90>

												DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"Got basic report from node %i, instance %i, value: %i",(unsigned char)frame[3],(unsigned char)frame[7], (unsigned char) frame[10]);
												DCEcallback->SendLightChangedEvents ((unsigned char)frame[3], (unsigned char)frame[7], (unsigned char) frame[10]);

										        } else if ((frame[8] == COMMAND_CLASS_BASIC) && (frame[9] == BASIC_SET)) {
												DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"Got basic set from node %i, instance %i, value: %i",(unsigned char)frame[3],(unsigned char)frame[7], (unsigned char) frame[10]);
												DCEcallback->SendOnOffEvent ((unsigned char)frame[3],(unsigned char)frame[7],(unsigned char) frame[10]);
											}
										}
				*/
				break;

			case COMMAND_CLASS_VERSION:
				logInfo(LOG_INFO,"COMMAND_CLASS_VERSION\n");
				if (frame[8] == VERSION_REPORT) {
					sprintf(logstring,"REPORT: Lib.typ: 0x%x, Prot.Ver: 0x%x, Sub: 0x%x App.Ver: 0x%x, Sub: 0x%x\n",(unsigned char)frame[9], (unsigned char)frame[10], (unsigned char)frame[11], (unsigned char)frame[12], (unsigned char)frame[13]);
					logInfo(LOG_INFO,logstring);
				} else {
					logInfo(LOG_WARN,"Unhandled VERSION request\n");
				}
				break;

			case COMMAND_CLASS_METER:
				logInfo(LOG_INFO,"COMMAND_CLASS_METER\n");
				if (frame[8] == METER_REPORT) {
					sprintf(logstring,"Got meter report from node %i\n",(unsigned char)frame[5]);
					logInfo(LOG_INFO,logstring);
					int scale = ( (unsigned char)frame[8] & METER_REPORT_SCALE_MASK ) >> METER_REPORT_SCALE_SHIFT;
					int precision = ( (unsigned char)frame[8] & METER_REPORT_PRECISION_MASK ) >> METER_REPORT_PRECISION_SHIFT;
					int size = (unsigned char)frame[8] & METER_REPORT_SIZE_MASK;
					int value;
					short tmpval;
					switch(size) {
					case 1:
						value = (signed char)frame[11];
						break;

					case 2:
						tmpval = ((unsigned char)frame[11] << 8) + (unsigned char)frame[10];
						value = (signed short)tmpval;
						break;

					default:
						value = ( (unsigned char)frame[11] << 24 ) + ( (unsigned char)frame[12] << 16 ) + ( (unsigned char)frame[13] << 8 ) + (unsigned char)frame[14];
						value = (signed int)value;
						break;
					}

					sprintf(logstring,"METER DEBUG: precision: %i scale: %i size: %i value: %i\n",precision,scale,size,value);
					logInfo(LOG_ALL,logstring);
					if (precision > 0) {
						value = value / pow(10 , precision) ;    // we only take the integer part for now
					}
					switch(((unsigned char)frame[9]) & 0x1f) { // meter type
					case METER_REPORT_ELECTRIC_METER:
						switch (scale) {
						case 0:
							sprintf(logstring,"Electric meter measurement received: %d kWh\n",value);
							logInfo(LOG_INFO,logstring);
							break;
						case 1:
							sprintf(logstring,"Electric meter measurement received: %d kVAh\n",value);
							logInfo(LOG_INFO,logstring);
							break;
						case 2:
							sprintf(logstring,"Electric meter measurement received: %d W\n",value);
							logInfo(LOG_INFO,logstring);
							break;
						case 3:
							sprintf(logstring,"Electric meter measurement received: %d pulses\n",value);
							logInfo(LOG_INFO,logstring);
							break;
						}
						break;

					default:
						sprintf(logstring,"unknown METER_REPORT received: %i\n",(unsigned char)frame[7]);
						logInfo(LOG_INFO,logstring);
						break;
					}
				} else {
					logInfo(LOG_WARN,"Unhandled METER request\n");
				}
				break;

			case COMMAND_CLASS_MANUFACTURER_SPECIFIC:
				logInfo(LOG_INFO,"COMMAND_CLASS_MANUFACTURER_SPECIFIC\n");
				if (frame[8] == MANUFACTURER_SPECIFIC_REPORT) {
					int manuf=0;
					int prod=0;
					int type=0;

					manuf = ((unsigned char)frame[9]<<8) + (unsigned char)frame[10];
					type = ((unsigned char)frame[11]<<8) + (unsigned char)frame[12];
					prod = ((unsigned char)frame[13]<<8) + (unsigned char)frame[14];
					sprintf(logstring,"REPORT: Manuf: 0x%x, Prod Typ: 0x%x, Prod 0x%x", manuf,type,prod);
					logInfo(LOG_INFO,logstring);
					//parseManufacturerSpecific((unsigned char)frame[3],manuf,type,prod);
				} else {
					logInfo(LOG_WARN,"Unhandled MANUFACTURER SPECIFIC request\n");
				}
				break;

			case COMMAND_CLASS_WAKE_UP:
				logInfo(LOG_INFO,"COMMAND_CLASS_WAKE_UP - \n");
				/*						// 0x1 0x8 0x0 0x4 0x4 0x2 0x2 0x84 0x7 0x74 (#########t)
										// if ((COMMAND_CLASS_WAKE_UP == frame[5]) && ( frame[6] == WAKE_UP_NOTIFICATION)) {
										if (frame[6] == WAKE_UP_NOTIFICATION) {
											// we got a wake up frame, make sure we remember the device does not always listen
											ZWNodeMapIt = ZWNodeMap.find((unsigned int)frame[3]);
											if (ZWNodeMapIt != ZWNodeMap.end()) {
												(*ZWNodeMapIt).second->sleepingDevice=true;
												if ((*ZWNodeMapIt).second->typeGeneric == GENERIC_TYPE_SENSOR_MULTILEVEL) {
												        // use multi instance get for value > 0, else use normal sensor report
												        if ((*ZWNodeMapIt).second->mapCCInstanceCount.find(COMMAND_CLASS_SENSOR_MULTILEVEL) != (*ZWNodeMapIt).second->mapCCInstanceCount.end() &&
													    (*ZWNodeMapIt).second->mapCCInstanceCount[COMMAND_CLASS_SENSOR_MULTILEVEL] > 0) {
													  DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"Requesting SENSOR_REPORT for instances 1 to %d, node %d",(*ZWNodeMapIt).second->mapCCInstanceCount[COMMAND_CLASS_SENSOR_MULTILEVEL],frame[3]);
													        for (int i = 1; i <= (*ZWNodeMapIt).second->mapCCInstanceCount[COMMAND_CLASS_SENSOR_MULTILEVEL]; i++) {
														        zwRequestMultilevelSensorReportInstance((unsigned int)frame[3], i);
														}
													} else {
													        zwRequestMultilevelSensorReport((unsigned int)frame[3]);
													}
												}
											}

											// read battery level
											zwGetBatteryLevel((unsigned char) frame[3]);

											// inject commands from the sleeping queue for this nodeid
											wakeupHandler((unsigned char) frame[3]);

											// handle broadcasts from unconfigured devices
											if (frame[2] & RECEIVE_STATUS_TYPE_BROAD ) {
												DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"Got broadcast wakeup from node %i, doing WAKE_UP_INTERVAL_SET",frame[3]);
												if (ournodeid != -1) {
													zwWakeupSet((unsigned char)frame[3],60,false); // wakeup interval
													wakeupHandler((unsigned char) frame[3]); // fire commands, device is awake
												}
											} else {
												DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"Got unicast wakeup from node %i, doing WAKE_UP_NO_MORE_INFORMATION",frame[3]);
												// send to sleep
												tempbuf[0]=FUNC_ID_ZW_SEND_DATA;
												tempbuf[1]=frame[3]; // destination
												tempbuf[2]=2; // length
												tempbuf[3]=COMMAND_CLASS_WAKE_UP;
												tempbuf[4]=WAKE_UP_NO_MORE_INFORMATION;
												tempbuf[5]=TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
												sendFunction( tempbuf , 6, REQUEST, 1);
												// ...
											}//
										}
				*/
				break;

			case COMMAND_CLASS_SENSOR_BINARY:
				logInfo(LOG_INFO,"COMMAND_CLASS_SENSOR_BINARY - \n");
				if (frame[8] == SENSOR_BINARY_REPORT) {
					sprintf(logstring,"Got sensor report from node %i, level: %i\n",(unsigned char)frame[5],(unsigned char)frame[9]);
					if ((unsigned char)frame[7] == 0xff) {
						//	DCEcallback->SendSensorTrippedEvents (frame[3], -1, true);
					} else {
						//	DCEcallback->SendSensorTrippedEvents (frame[3], -1, false);
					}
				} else {
					logInfo(LOG_WARN,"Unhandled SENSOR_BINARY request\n");
				}
				break;

			case COMMAND_CLASS_SENSOR_MULTILEVEL:
				logInfo(LOG_INFO,"COMMAND_CLASS_SENSOR_MULTILEVEL - \n");
				if (frame[8] == SENSOR_MULTILEVEL_REPORT) {
					handleCommandSensorMultilevelReport((unsigned int)frame[5], // node id
					                                    -1, // instance id
					                                    (unsigned int)frame[9], // sensor type
					                                    (unsigned int)frame[10], // value metadata
					                                    (unsigned int)frame[11], // value
					                                    (unsigned int)frame[12],
					                                    (unsigned int)frame[13],
					                                    (unsigned int)frame[14]);
				} else {
					logInfo(LOG_WARN,"Unhandled SENSOR_MULTILEVEL request\n");
				}
				break;

			case COMMAND_CLASS_SENSOR_ALARM:
				logInfo(LOG_INFO,"COMMAND_CLASS_SENSOR_ALARM - \n");
				/*						if ((unsigned char)frame[6] == SENSOR_ALARM_REPORT) {
											DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"Got sensor report from node %i, source node ID: %i, sensor type: %i, sensor state: %i",(unsigned char)frame[3],(unsigned char)frame[7],(unsigned char)frame[8],(unsigned char)frame[9]);
											if ((unsigned char)frame[9]!=0) { // ALARM!!!
												/* if ((unsigned char)frame[0]==0xff) {

												} else { // percentage of severity

												}
												switch ((unsigned char)frame[8]) {
													case 0x0: // general purpose
														break;
													case 0x1: // smoke alarm
														DCEcallback->SendFireAlarmEvent (frame[3], -1);
														break;
													case 0x2: // co alarm
														DCEcallback->SendAirQualityEvent (frame[3], -1);
														break;
													case 0x3: // co2 alarm
														DCEcallback->SendAirQualityEvent (frame[3], -1);
														break;
													case 0x4: // heat alarm
														DCEcallback->SendFireAlarmEvent (frame[3], -1);
														break;
													case 0x5: // water leak alarm
														break;
												}
											}

										} */
				break;

			case COMMAND_CLASS_SWITCH_MULTILEVEL:
				logInfo(LOG_INFO,"COMMAND_CLASS_SWITCH_MULTILEVEL - \n");
				if (frame[8] == SWITCH_MULTILEVEL_REPORT) {
					sprintf(logstring,"Got switch multilevel report from node %i, level: %i\n",(unsigned char)frame[5],(unsigned char)frame[9]);
					logInfo(LOG_INFO,logstring);
				} else if ((unsigned char)frame[6] == SWITCH_MULTILEVEL_SET) {
					sprintf(logstring,"Got switch multilevel set from node %i, level: %i\n",(unsigned char)frame[5],(unsigned char)frame[9]);
					logInfo(LOG_INFO,logstring);
				} else {
					logInfo(LOG_WARN,"Unhandled SWITCH_MULTILEVEL request\n");
				}
				break;

			case COMMAND_CLASS_SWITCH_ALL:
				logInfo(LOG_INFO,"COMMAND_CLASS_SWITCH_ALL - \n");
				if (frame[8] == SWITCH_ALL_ON) {
					sprintf(logstring,"Got switch all ON from node %i\n",(unsigned char)frame[5]);
					logInfo(LOG_INFO,logstring);
				} else if (frame[8] == SWITCH_ALL_OFF) {
					sprintf(logstring,"Got switch all OFF from node %i\n",(unsigned char)frame[5]);
					logInfo(LOG_INFO,logstring);
				} else {
					logInfo(LOG_WARN,"Unhandled SWITCH_ALL request\n");
				}
				break;

			case COMMAND_CLASS_BASIC:
				logInfo(LOG_INFO,"COMMAND_CLASS_BASIC - \n");
				if (frame[8] == BASIC_REPORT) {
					sprintf(logstring,"Got basic report from node %i, value: %i\n",(unsigned char)frame[5],(unsigned char)frame[9]);
					logInfo(LOG_INFO,logstring);
				} else if ((unsigned char)frame[8] == BASIC_SET) {
					sprintf(logstring,"Got BASIC_SET from node %i, value %i\n",(unsigned char)frame[5],(unsigned char)frame[9]);
					logInfo(LOG_INFO,logstring);
				} else {
					sprintf(logstring,"Got COMMAND_CLASS_BASIC: %i, ignoring\n",(unsigned char)frame[8]);
					logInfo(LOG_WARN,logstring);
				}
				break;

			case COMMAND_CLASS_ASSOCIATION:
				logInfo(LOG_INFO,"COMMAND_CLASS_ASSOCIATION - \n");
				if (frame[8] == ASSOCIATION_REPORT) {
					int tmp_group = (unsigned char)frame[9];
					sprintf(logstring,"\tAssociations for group: %i\n",tmp_group);
					logInfo(LOG_INFO,logstring);
					sprintf(logstring,"\tMax nodes supported: %i\n",(unsigned char)frame[10]);
					logInfo(LOG_INFO,logstring);
					sprintf(logstring,"\tReports to follow: %i\n",(unsigned char)frame[11]);
					logInfo(LOG_INFO,logstring);
					if ((length-10)>0) {
						logInfo(LOG_INFO,"\tNodes: ");
						unsigned int i;
						for (i=0; i<(length-10); i++) {
							sprintf(logstring,"%i",(unsigned char)frame[12+i]);
//							if (tmp_nodelist.length()!=0) {
//								tmp_nodelist += ',';
//							}
//							char tmp_string[1024];
//							sprintf(tmp_string,"%d",(unsigned char)frame[12+i]);
//							tmp_nodelist += tmp_string;
						}
						//DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"Built nodelist: %s",tmp_nodelist.c_str());
					}
				} else {
					logInfo(LOG_WARN,"Unhandled ASSOCIATION request\n");
				}
				break;

			case COMMAND_CLASS_BATTERY:
				if ((unsigned char)frame[6] == BATTERY_REPORT) {
					sprintf(logstring,"COMMAND_CLASS_BATTERY:BATTERY_REPORT: Battery level: %d\n",(unsigned char)frame[7]);
					logInfo(LOG_INFO,logstring);

					if ((unsigned char)frame[7] == 0xff) {
						sprintf(logstring,"Battery low warning from node %d\n",(unsigned char)frame[3]);
						logInfo(LOG_INFO,logstring);
					}
					//DCEcallback->ReportBatteryStatus((unsigned char) frame[3], (unsigned char) frame[7]);
				} else {
					logInfo(LOG_WARN,"Unhandled BATTERY request\n");
				}
				break;

			case COMMAND_CLASS_SWITCH_BINARY:
				if ((unsigned char)frame[8] == SWITCH_BINARY_SET) {
					sprintf(logstring,"COMMAND_CLASS_SWITCH_BINARY:SWITCH_BINARY_SET received from node %d value %d",(unsigned char)frame[5],(unsigned char)frame[9]);
					logInfo(LOG_INFO,logstring);
				} else {
					logInfo(LOG_WARN,"Unhandled SWITCH_BINARY request\n");
				}
				break;

			case COMMAND_CLASS_MULTI_CMD:
				logInfo(LOG_INFO,"COMMAND_CLASS_MULTI_CMD - \n");
				/*					if ((unsigned char)frame[6] == MULTI_CMD_ENCAP) {
										time_t timestamp;
										struct tm *timestruct;
										int offset = 0;
										// int cmd_length = 0;
										// int cmd_count = 0;

										DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"Got encapsulated multi command from node %i",(unsigned char)frame[3]);
										timestamp = time(NULL);
										timestruct = localtime(&timestamp);
										// printf("Time: %i %i %i\n",timestruct->tm_wday, timestruct->tm_hour, timestruct->tm_min);
										// iterate over commands
										offset = 8;
										for (int i=0; i<frame[7]; i++) {
											DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"COMMAND LENGTH: %d, CLASS: %x",frame[offset],(unsigned char) frame[offset+1]);
											switch ((unsigned char)frame[offset+1]) {
											case COMMAND_CLASS_BATTERY:
												if (BATTERY_REPORT == (unsigned char) frame[offset+2]) {
													DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"COMMAND_CLASS_BATTERY:BATTERY_REPORT: Battery level: %d",(unsigned char)frame[offset+3]);
													if ((unsigned char)frame[offset+3] == 0xff) {
														DCE::LoggerWrapper::GetInstance()->Write(LV_WARNING,"Battery low warning from node %d",(unsigned char)frame[3]);
													}
													DCEcallback->ReportBatteryStatus((unsigned char) frame[3], (unsigned char) frame[offset+3]);
												}
												break;

											case COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE:
												if (SCHEDULE_CHANGED_GET == (unsigned char) frame[offset+2]) {
													DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE:SCHEDULE_CHANGED_GET");
												}
												if (SCHEDULE_OVERRIDE_GET == (unsigned char) frame[offset+2]) {
													DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE:SCHEDULE_OVERRIDE_GET");
												}
												if (SCHEDULE_OVERRIDE_REPORT == (unsigned char) frame[offset+2]) {
													DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE:SCHEDULE_OVERRIDE_REPORT: Setback state: %i",(unsigned char)frame[offset+4]);
													// update basic device state in map
													ZWNodeMapIt = ZWNodeMap.find((unsigned int)frame[3]);
													if (ZWNodeMapIt != ZWNodeMap.end()) {
														(*ZWNodeMapIt).second->stateBasic = (unsigned char)frame[offset+4]==0 ? 0xff : 0x0;
													}
												}
												break;

											case COMMAND_CLASS_CLOCK:
												if (CLOCK_GET == (unsigned char) frame[offset+2]) {
													DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"COMMAND_CLASS_CLOCK:CLOCK_GET");
												}
												break;

											case COMMAND_CLASS_WAKE_UP:
												if (WAKE_UP_NOTIFICATION == (unsigned char) frame[offset+2]) {
													DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"COMMAND_CLASS_WAKE_UP:WAKE_UP_NOTIFICATION");
												}
												break;

											default:
											}
											offset += frame[offset]+1;
										}

										tempbuf[0]=FUNC_ID_ZW_SEND_DATA;
										tempbuf[1]=frame[3]; // node id
										tempbuf[2]=19; // cmd_length;
										tempbuf[3]=COMMAND_CLASS_MULTI_CMD;
										tempbuf[4]=MULTI_CMD_ENCAP;
										tempbuf[5]=4; // cmd_count;
										tempbuf[6]=4; //length of next command
										tempbuf[7]=COMMAND_CLASS_CLOCK;
										tempbuf[8]=CLOCK_REPORT;
										tempbuf[9]=((timestruct->tm_wday == 0 ? 7 : timestruct->tm_wday) << 5 ) | timestruct->tm_hour;
										tempbuf[10]=timestruct->tm_min == 60 ? 0 : timestruct->tm_min;
										tempbuf[11]=3; // length of next command
										tempbuf[12]=COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE;
										tempbuf[13]=SCHEDULE_CHANGED_REPORT;
										string sSchedule = DCEcallback->GetSchedule((unsigned char) frame[3]);
										int iScheduleVersion = 0;
										if (sSchedule.find(";") != string::npos) {
											vector<string> vectSchedule;
											StringUtils::Tokenize(sSchedule, ";", vectSchedule);
											if (vectSchedule[0].find(",") !=string::npos) {
												vector<string> vectScheduleVersion;
												StringUtils::Tokenize(vectSchedule[0], ",", vectScheduleVersion);
												iScheduleVersion = atoi(vectScheduleVersion[1].c_str());
												DCE::LoggerWrapper::GetInstance()->Write(LV_ZWAVE,"Schedule Version: %i",iScheduleVersion);
											}
										}

										tempbuf[14]=iScheduleVersion; // dummy schedule version for now
										tempbuf[15]=3; // length of next command
										tempbuf[16]=COMMAND_CLASS_BASIC;
										tempbuf[17]=BASIC_SET;
										ZWNodeMapIt = ZWNodeMap.find((unsigned int)frame[3]);
										if (ZWNodeMapIt != ZWNodeMap.end()) {
											tempbuf[18]=(*ZWNodeMapIt).second->stateBasic;
										} else {
											tempbuf[18]=0x00;
										}
										tempbuf[19]=2; //length of next command
										tempbuf[20]=COMMAND_CLASS_WAKE_UP;
										tempbuf[21]=WAKE_UP_NO_MORE_INFORMATION;
										tempbuf[22]=TRANSMIT_OPTION_ACK | TRANSMIT_OPTION_AUTO_ROUTE;
										sendFunction( tempbuf , 23, REQUEST, 1);

									}
				*/				break;

			default:
				sprintf(logstring,"Function not implemented - unhandled command class: %x\n",(unsigned char)frame[7]);
				logInfo(LOG_INFO,logstring);
				break;

			}
			break;

		default:
			sprintf(logstring,"Unhandled REQUEST frame: 0x%02X (callback ID 0x%02X)\n", (unsigned char)frame[3], (unsigned char)frame[4]);
			logInfo(LOG_WARN,logstring);
			break;
		}
	} else {
		logInfo(LOG_ERROR,"Invalid frame received (not REQUEST or RESPONSE)\n");
		return 0;
	}
}


void sendFunction (char *buffer, size_t length, int type, int response)
{

	int index = 0;
	unsigned int i = 0;
	char tempbyte[16];

	pthread_mutex_lock(&mutexSendQueue);

	//Build frame
	zwCommandQueue[zwQueueIndex].buffer[index++] = SOF;
	zwCommandQueue[zwQueueIndex].buffer[index++] = length + 2 + (response ? 1 : 0);
	zwCommandQueue[zwQueueIndex].buffer[index++] = type == RESPONSE ? RESPONSE : REQUEST;
	for(i=0; i<length; i++) {
		zwCommandQueue[zwQueueIndex].buffer[index++] = buffer[i];
	}

	zwCommandQueue[zwQueueIndex].await_response=response;
	zwCommandQueue[zwQueueIndex].sendcount = 0;

	//Set callback
	if (response) {
		callbackid = getCallbackID();
		zwCommandQueue[zwQueueIndex].buffer[index++] = callbackid;
	} else {
		callbackid = 0;
	}
	zwCommandQueue[zwQueueIndex].await_response = callbackid;
	zwCommandQueue[zwQueueIndex].callback_type = buffer[0];
	zwCommandQueue[zwQueueIndex].callbackid = callbackid;

	zwCommandQueue[zwQueueIndex].buffer[index] = checksum(zwCommandQueue[zwQueueIndex].buffer,zwCommandQueue[zwQueueIndex].buffer[1]);
	zwCommandQueue[zwQueueIndex].len = zwCommandQueue[zwQueueIndex].buffer[1]+2;
	zwQueueIndex++;

	pthread_mutex_unlock(&mutexSendQueue);

	sprintf(logstring, "Queuing %i bytes in location %i: ", index+1,zwQueueIndex-1);
	for(i=0; i <= index; i++) {
		sprintf(tempbyte,"0x%02X ",(unsigned char)zwCommandQueue[zwQueueIndex-1].buffer[i]);
		strncat(logstring,tempbyte,5);
	}
	strcat(logstring, "\n");
	logInfo(LOG_ALL,logstring);

}


void *receiveFunction(void)
{

// Serial receive function.  Instantiated in a new thread.

	char retbuf[1];
	char receivebuf[512];
	int len1;
	int len2;
	char strbyte[16];
	int i;
	int idletimer;	//Time since last frame received
	int commandtimer;  //Time since command sent

	idletimer = 0;

	while(1) {
		receivebuf[0] = 0;
		len2 = ReadSerialStringEx(h_controller,receivebuf,1,100);
		if (len2 == 1) {
			switch(receivebuf[0]) {
			case SOF:  //Found start of frame
				// If we await an ACK instead, trigger resend
				if (await_ack) {
					logInfo(LOG_WARN, "Error: SOF found\n");
					await_ack = 0;
				}
				// Read the length byte
				len2 += ReadSerialStringEx(h_controller,receivebuf+1,1, 100);

				// Read the rest of the frame
				len2 += ReadSerialStringEx(h_controller,receivebuf+2,receivebuf[1], 100);

				if (len2>0) {
					sprintf(logstring,"Frame received with %i bytes: ", len2);
					for(i=0; i < len2; i++) {
						sprintf(strbyte,"0x%02X ",receivebuf[i]);
						strncat(logstring,strbyte,5);
					}
					strcat(logstring, "\n");
					logInfo(LOG_ALL,logstring);
				}

				if ((unsigned char)receivebuf[len2-1] == (unsigned char)checksum(receivebuf,receivebuf[1])) {
					logInfo(LOG_ALL,"Checksum correct.  Sending ACK\n");
					retbuf[0] = ACK;
					WriteSerialStringEx(h_controller,retbuf,1);
					decodeFrame(receivebuf,len2);
				} else {
					sprintf(logstring,"Checksum incorrect.  Expecting 0x%02X but got 0x%02X.  Trigger resend\n", (unsigned char)checksum(receivebuf,receivebuf[1]),(unsigned char)receivebuf[len2-1]);
					logInfo(LOG_WARN,logstring);
					retbuf[0] = NAK;
					WriteSerialStringEx(h_controller,retbuf,1);
				}
				break;

			case CAN: //Found cancel.  Trigger resend
				logInfo(LOG_INFO,"CAN received\n");
				await_ack = 0;
				break;

			case NAK:  //Found negative ACK.  Trigger resend
				logInfo(LOG_INFO,"NAK received\n");
				await_ack = 0;
				break;

			case ACK:  //Found ACK
				logInfo(LOG_ALL,"ACK received\n");
				if (await_ack != 0)
					await_ack = 0;
				if (await_callback == 0) {
					pthread_mutex_lock(&mutexSendQueue);
					zwPopJob();
					pthread_mutex_unlock(&mutexSendQueue);
				}
				break;

			default:
				sprintf(logstring,"ERROR: Out of frame sync byte: 0x%02X\n",receivebuf[0]);
				logInfo(LOG_WARN, logstring);
				break;
			}
			idletimer = 0;	// At least 1 byte of data received.  Reset idle timer.
		} else {
			//Nothing received.  Anything to send?
			pthread_mutex_lock(&mutexSendQueue);

			if (zwQueueIndex>0) {
				idletimer=0;

				if ( await_ack != 1 && await_callback == 0) {
					await_callback = (unsigned int) zwCommandQueue[0].callbackid;
					callback_type = (unsigned int) zwCommandQueue[0].callback_type;
					len1 = zwCommandQueue[0].len;

					WriteSerialStringEx(h_controller,zwCommandQueue[0].buffer, len1);
					zwCommandQueue[0].sendcount++;
					await_ack = 1;
					commandtimer=0;

					sprintf(logstring, "Writing %i bytes.  Attempt: %i.  Frame: ", len1,zwCommandQueue[0].sendcount);
					for(i=0; i < len1; i++) {
						sprintf(strbyte,"0x%02X ",(unsigned char)zwCommandQueue[0].buffer[i]);
						strncat(logstring,strbyte,5);

					}
					strcat(logstring, "\n");
					logInfo(LOG_ALL,logstring);

				} else {
					commandtimer++;
					if (commandtimer > 30 && await_callback != 0) {
						sprintf(logstring, "No callback received for CB 0x%02X\n", await_callback);
						logInfo(LOG_WARN,logstring);
						commandtimer = 0;
						// resend, we got no final callback
						await_ack = 0;
						await_callback = 0;
						if (zwCommandQueue[0].sendcount > 2) {
							logInfo(LOG_WARN, "Command timout.  Dropping command.\n");
							zwPopJob();
						}
					}
				}

			} else {
				idletimer++;
			}

			pthread_mutex_unlock(&mutexSendQueue);

			// we have been idle for 30 seconds, let's poll the devices
			if ((idletimer > 300) && poll_state) {
				sprintf(logstring, "We have been idle for %i seconds, polling devices\n", idletimer/10);
				logInfo(LOG_ALL,logstring);
				zwRequestBasicReport(NODE_BROADCAST,0);
				zwRequestBasicReport(NODE_BROADCAST,2);
				zwRequestMultilevelSensorReport(NODE_BROADCAST);
			}
		}
	}
}


char checksum(char *buff, int len)
{

	//Calculate checksum

	int loop;
	char checksum = 0xff;

	for(loop = 1; loop<=len; loop++)
		checksum ^= buff[loop];

	return checksum;
}


unsigned char getCallbackID(void)
{

	// Return callback ID for command

	if (callbackPool > 254 || callbackPool < 1)
		callbackPool = 1;
	return callbackPool++;
}


void zwPopJob (void)
{

	//Remove the command at the top of the queue

	int i;

	for (i=1; i<zwQueueIndex; i++)
		zwCommandQueue[i-1] = zwCommandQueue[i];

	zwQueueIndex--;
	sprintf(logstring,"Queue length: %i\n",zwQueueIndex);
	logInfo(LOG_ALL, logstring);
}


void addIntent(int nodeid, int type)
{

	pthread_mutex_lock (&mutexSendQueue);
	zwIntentQueue[zwIntentIndex].nodeid=nodeid;
	zwIntentQueue[zwIntentIndex++].type = type;
	pthread_mutex_unlock (&mutexSendQueue);
}


int getIntent(int type)
{

	int nodeid = -1;
	int i;

	if (zwIntentIndex>0) {
		nodeid = zwIntentQueue[0].nodeid;
		pthread_mutex_lock (&mutexSendQueue);
		for (i=1; i<zwIntentIndex; i++)
			zwIntentQueue[i-1] = zwIntentQueue[i];
		zwIntentIndex--;
		pthread_mutex_unlock (&mutexSendQueue);
	}
	return nodeid;
}


void handleCommandSensorMultilevelReport(int nodeid, int instance_id, int sensortype, int metadata, int val1, int val2, int val3, int val4)
{
	int scale, precision, size;
	int value;
	short tmpval;
	float fValue;

	sprintf(logstring,"\tGot sensor report from node %i, instance %i\n",nodeid, instance_id);
	logInfo(LOG_INFO,logstring);
	scale = ( (unsigned char)metadata & SENSOR_MULTILEVEL_REPORT_SCALE_MASK ) >> SENSOR_MULTILEVEL_REPORT_SCALE_SHIFT;
	precision = ( (unsigned char)metadata & SENSOR_MULTILEVEL_REPORT_PRECISION_MASK ) >> SENSOR_MULTILEVEL_REPORT_PRECISION_SHIFT;
	size = (unsigned char)metadata & SENSOR_MULTILEVEL_REPORT_SIZE_MASK;

	switch(size) {
	case 1:
		value = (signed char)val1;
		break;

	case 2:
		tmpval = ((unsigned char)val1 << 8) + (unsigned char)val2;
		value = (signed short)tmpval;
		break;

	default:
		value = ( (unsigned char)val1 << 24 ) + ( (unsigned char)val2 << 16 ) + ( (unsigned char)val3 << 8 ) + (unsigned char)val4;
		value = (signed int)value;
		break;
	}

	sprintf(logstring,"\tMULTILEVEL DEBUG: precision: %i scale: %i, size: %i, value: %i\n",precision,scale,size,value);
	logInfo(LOG_ALL,logstring);

	if (precision > 0) {
		fValue = value / pow(10,precision);
		sprintf(logstring,"\tMULTILEVEL DEBUG: float value: %2f\n",fValue);
		logInfo(LOG_ALL,logstring);
	}  else {
		fValue = value;
	}

	if (precision > 0) {
		value = value / pow(10 , precision) ;    // we only take the integer part for now
	}

	switch(sensortype) { // sensor type
	case SENSOR_MULTILEVEL_REPORT_GENERAL_PURPOSE_VALUE:
		if (scale == 0) {
			sprintf(logstring,"\tGeneral purpose measurement value received from node %i: %d%%\n",value);
			logInfo(LOG_INFO,logstring);
		} else {
			sprintf(logstring,"\tGeneral purpose measurement value received from node %i: %d (dimensionless)\n",value);
			logInfo(LOG_INFO,logstring);
		}
		break;

	case SENSOR_MULTILEVEL_REPORT_LUMINANCE:
		if (scale == 0) {
			sprintf(logstring,"\tLuminance measurement received: %d%%\n",value);
			logInfo(LOG_INFO,logstring);
		} else {
			sprintf(logstring,"\tLuminance measurement received: %d Lux\n",value);
			logInfo(LOG_INFO,logstring);
		}
//		DCEcallback->SendBrightnessChangedEvent ((unsigned char)nodeid, instance_id, value);
		break;

	case SENSOR_MULTILEVEL_REPORT_POWER:
		sprintf(logstring,"\tMULTILEVEL DEBUG: node: %i precision: %i scale: %i size: %i value: %i\n",nodeid,precision,scale,size,value);
		logInfo(LOG_ALL,logstring);
		if (scale == 0) {
			sprintf(logstring,"\tPower level measurement received from node %i: %d W\n",nodeid, value);
			logInfo(LOG_WARN,logstring);
		} else {
			sprintf(logstring,"\tPower level measurement received from node %i: %d\n",nodeid, value);
			logInfo(LOG_WARN,logstring);
		}
//			DCEcallback->SendPowerUsageChangedEvent((unsigned char)nodeid, instance_id, value);
		break;

	case SENSOR_MULTILEVEL_REPORT_CO2_LEVEL:
		sprintf(logstring,"\tCO2 level measurement received: %d ppm",value);
		logInfo(LOG_INFO,logstring);
//		DCEcallback->SendCO2LevelChangedEvent ((unsigned char)nodeid, instance_id, value);
		break;

	case SENSOR_MULTILEVEL_REPORT_RELATIVE_HUMIDITY:
		sprintf(logstring,"\tRelative humidity measurement received: %.2f%%\n",fValue);
		logInfo(LOG_INFO,logstring);
//		DCEcallback->SendHumidityChangedEvent ((unsigned char)nodeid, instance_id, fValue);
		break;

	case SENSOR_MULTILEVEL_REPORT_TEMPERATURE:
		if (scale == 0) {
			sprintf(logstring,"\tTemperature level measurement received: %.2f C\n",fValue);
			logInfo(LOG_INFO,logstring);
//			DCEcallback->SendTemperatureChangedEvent ((unsigned char)nodeid, instance_id, fValue);
		} else {
			sprintf(logstring,"\tTemperature level measurement received: %.2f F\n",fValue);
			logInfo(LOG_INFO,logstring);
//			DCEcallback->SendTemperatureChangedEvent ((unsigned char)nodeid, instance_id, (fValue-32) *5 / 9);
		}
		break;

	default:
		sprintf(logstring,"\tSensor type 0x%x not handled",(unsigned char)sensortype);
		logInfo(LOG_INFO,logstring);
		break;
	}
}


void zwReadMemory(int offset, int len)
{

	sprintf(logstring, "Reading eeprom at offset %i\n",offset);
	logInfo(LOG_INFO, logstring);
	sendbuf[0] = 0x23;
	sendbuf[1] = (offset >> 8) & 0xff;
	sendbuf[2] = offset & 0xff;;
	sendbuf[3] = len & 0xff;
	sendFunction( sendbuf , 4, REQUEST, 0);
}


void zwWriteMemory(int offset, int len, unsigned char *data)
{

	int i;

	sprintf(logstring, "Writing eeprom at offset %i\n",offset);
	logInfo(LOG_INFO, logstring);
	sendbuf[0] = 0x24; // MemoryPutBuffer
	sendbuf[1] = (offset >> 8) & 0xff;
	sendbuf[2] = offset & 0xff;;
	sendbuf[3] = (len >> 8) & 0xff;
	sendbuf[4] = len & 0xff;
	if (len > 100) {
		logInfo(LOG_INFO, "length too long\n");
	} else {
		for (i=0; i<len; i++) {
			sendbuf[5+i]=data[i];
		}
	}
	sendFunction( sendbuf , 5+len, REQUEST, 0);
}
