/************************************************************************************
*
*		ZWApi.h
*
*		Z-Wave utility.  Z-Wave API.
*
*************************************************************************************/

#ifndef _ZWAPI_H_
#define _ZWAPI_H_

#define LOG_LEVEL		LOG_ALL
\
#define MAGIC_LEN																	0x1D

#define SOF																				0x01
#define ACK																				0x06
#define NAK																				0x15
#define CAN																				0x18

#define TRANSMIT_OPTION_ACK         										0x01
#define TRANSMIT_OPTION_LOW_POWER   								0x02
#define TRANSMIT_OPTION_AUTO_ROUTE  								0x04
#define TRANSMIT_OPTION_FORCE_ROUTE 								0x08
#define TRANSMIT_COMPLETE_OK      										0x00
#define TRANSMIT_COMPLETE_NO_ACK  									0x01
#define TRANSMIT_COMPLETE_FAIL    										0x02
#define TRANSMIT_COMPLETE_NOROUTE 									0x04

#define	REQUEST																		0x00
#define	RESPONSE																	0x01

#define NODE_BROADCAST															0xff

#define COMMAND_CLASS_BASIC												0x20
#define COMMAND_CLASS_SWITCH_BINARY								0x25
#define COMMAND_CLASS_SWITCH_MULTILEVEL  				    	0x26
#define COMMAND_CLASS_SWITCH_ALL										0x27
#define COMMAND_CLASS_MULTI_INSTANCE_ASSOCIATION		0x8E
#define COMMAND_CLASS_MULTI_INSTANCE								0x60
#define COMMAND_CLASS_SENSOR_MULTILEVEL						0x31
#define COMMAND_CLASS_METER				0x32
#define COMMAND_CLASS_SECURITY				0x98
#define COMMAND_CLASS_ASSOCIATION			0x85
#define COMMAND_CLASS_MULTI_CMD                         0x8F
#define COMMAND_CLASS_SWITCH_ALL			0x27
#define COMMAND_CLASS_WAKE_UP                         	0x84
#define COMMAND_CLASS_CONTROLLER_REPLICATION          	0x21
#define COMMAND_CLASS_SENSOR_BINARY			0x30
#define COMMAND_CLASS_VERSION				0x86
#define COMMAND_CLASS_SENSOR_ALARM			0x9c

#define CTRL_REPLICATION_TRANSFER_GROUP              	0x31

#define VERSION_GET					0x11
#define VERSION_REPORT					0x12

#define SENSOR_BINARY_REPORT				0x03
#define ZW_GET_ROUTING_INFO			0x80
#define COMMAND_CLASS_BATTERY				0x80
#define BATTERY_GET					0x02
#define BATTERY_REPORT					0x03

#define SWITCH_MULTILEVEL_REPORT                        0x03
#define SWITCH_ALL_ON					0x04
#define SWITCH_ALL_OFF					0x05
#define SWITCH_BINARY_SET				0x01

#define COMMAND_CLASS_MANUFACTURER_SPECIFIC		0x72
#define MANUFACTURER_SPECIFIC_GET			0x04
#define MANUFACTURER_SPECIFIC_REPORT			0x05

#define BASIC_SET																	0x01
#define BASIC_GET																	0x02
#define SWITCH_MULTILEVEL_SET       				                 	0x01
#define SWITCH_MULTILEVEL_GET                       				 	0x02

#define UPDATE_STATE_NODE_INFO_RECEIVED     		0x84
#define UPDATE_STATE_NODE_INFO_REQ_FAILED		0x81
#define UPDATE_STATE_DELETE_DONE			0x20
#define UPDATE_STATE_NEW_ID_ASSIGNED			0x40

#define ZW_GET_VERSION															0x15
#define ZW_MEMORY_GET_ID														0x20	// response: 4byte home id, node id
#define ZW_MEM_GET_BUFFER													0x23
#define ZW_MEM_PUT_BUFFER													0x24
#define ZW_CLOCK_SET																0x30
#define ZW_SUC_FUNC_BASIC_SUC											0x00
#define ZW_SUC_FUNC_NODEID_SERVER									0x01

#define ASSOCIATION_SET					0x01
#define ASSOCIATION_GET					0x02
#define ASSOCIATION_REPORT				0x03
#define ASSOCIATION_REMOVE				0x04

#define ADD_NODE_ANY					0x01
#define ADD_NODE_STOP					0x05
#define ADD_NODE_STATUS_LEARN_READY          		0x01
#define ADD_NODE_STATUS_NODE_FOUND           		0x02
#define ADD_NODE_STATUS_ADDING_SLAVE         		0x03
#define ADD_NODE_STATUS_ADDING_CONTROLLER    		0x04
#define ADD_NODE_STATUS_PROTOCOL_DONE        		0x05
#define ADD_NODE_STATUS_DONE                 		0x06
#define ADD_NODE_STATUS_FAILED               		0x07
#define ADD_NODE_OPTION_HIGH_POWER			0x80

#define REMOVE_NODE_ANY					0x01
#define REMOVE_NODE_STOP				0x05
#define REMOVE_NODE_STATUS_LEARN_READY          	0x01
#define REMOVE_NODE_STATUS_NODE_FOUND           	0x02
#define REMOVE_NODE_STATUS_ADDING_SLAVE         	0x03
#define REMOVE_NODE_STATUS_ADDING_CONTROLLER    	0x04
#define REMOVE_NODE_STATUS_PROTOCOL_DONE        	0x05
#define REMOVE_NODE_STATUS_DONE                 	0x06
#define REMOVE_NODE_STATUS_FAILED               	0x07

#define METER_GET					0x01
#define METER_REPORT					0x02
#define METER_REPORT_ELECTRIC_METER			0x01
#define METER_REPORT_GAS_METER				0x02
#define METER_REPORT_WATER_METER			0x03

#define METER_REPORT_SIZE_MASK				0x07
#define METER_REPORT_SCALE_MASK				0x18
#define METER_REPORT_SCALE_SHIFT			0x03
#define METER_REPORT_PRECISION_MASK			0xe0
#define METER_REPORT_PRECISION_SHIFT			0x05

#define FUNC_ID_SERIAL_API_GET_INIT_DATA						0x02
#define FUNC_ID_SERIAL_API_GET_CAPABILITIES 				0x07
#define FUNC_ID_SERIAL_API_SOFT_RESET							0x08
#define FUNC_ID_APPLICATION_COMMAND_HANDLER             	0x04
#define FUNC_ID_ZW_APPLICATION_UPDATE                   		0x49
#define FUNC_ID_ZW_SET_DEFAULT											0x42
#define FUNC_ID_ZW_GET_NODE_PROTOCOL_INFO               	0x41
#define FUNC_ID_ZW_SEND_DATA                            					0x13
#define FUNC_ID_ZW_SET_LEARN_MODE                       			0x50
#define FUNC_ID_ZW_ASSIGN_SUC_RETURN_ROUTE					0x51
#define FUNC_ID_ZW_ENABLE_SUC                           				0x52
#define FUNC_ID_ZW_REQUEST_NETWORK_UPDATE					0x53
#define FUNC_ID_ZW_SET_SUC_NODE_ID                      			0x54
#define FUNC_ID_ZW_GET_SUC_NODE_ID                      			0x56
#define FUNC_ID_ZW_ADD_NODE_TO_NETWORK							0x4a
#define FUNC_ID_ZW_REMOVE_NODE_FROM_NETWORK				0x4b
#define FUNC_ID_ZW_REQUEST_NODE_INFO                    			0x60
#define FUNC_ID_ZW_REMOVE_FAILED_NODE_ID                		0x61
#define FUNC_ID_ZW_IS_FAILED_NODE_ID								0x62
#define FUNC_ID_ZW_ASSIGN_RETURN_ROUTE							0x46
#define FUNC_ID_ZW_REPLICATION_COMMAND_COMPLETE         0x44

#define MULTI_INSTANCE_VERSION											0x01
#define MULTI_INSTANCE_GET													0x04
#define MULTI_INSTANCE_CMD_ENCAP										0x06
#define MULTI_INSTANCE_REPORT											0x05
#define MULTI_INSTANCE_ASSOCIATION_VERSION              	0x01
#define MULTI_INSTANCE_ASSOCIATION_GET                 		0x02
#define MULTI_INSTANCE_ASSOCIATION_GROUPINGS_GET    	0x05
#define MULTI_INSTANCE_ASSOCIATION_GROUPINGS_REPORT     0x06
#define MULTI_INSTANCE_ASSOCIATION_REMOVE               	0x04
#define MULTI_INSTANCE_ASSOCIATION_REPORT               	0x03
#define MULTI_INSTANCE_ASSOCIATION_SET                  		0x01
#define MULTI_INSTANCE_ASSOCIATION_REMOVE_MARKER    	0x00
#define MULTI_INSTANCE_ASSOCIATION_REPORT_MARKER    	0x00
#define MULTI_INSTANCE_ASSOCIATION_SET_MARKER       	0x00

#define BASIC_TYPE_CONTROLLER                        					0x01
#define BASIC_TYPE_STATIC_CONTROLLER                    			0x02
#define BASIC_TYPE_SLAVE                                						0x03
#define BASIC_TYPE_ROUTING_SLAVE                        				0x04
#define GENERIC_TYPE_GENERIC_CONTROLLER                 		0x01
#define GENERIC_TYPE_STATIC_CONTROLLER                  		0x02
#define GENERIC_TYPE_AV_CONTROL_POINT                   		0x03
#define GENERIC_TYPE_DISPLAY                            					0x06
#define GENERIC_TYPE_GARAGE_DOOR                        				0x07
#define GENERIC_TYPE_THERMOSTAT                         				0x08
#define GENERIC_TYPE_WINDOW_COVERING                    			0x09
#define GENERIC_TYPE_REPEATER_SLAVE                     			0x0F
#define GENERIC_TYPE_SWITCH_BINARY                      			0x10
#define GENERIC_TYPE_SWITCH_MULTILEVEL                  		0x11
#define SPECIFIC_TYPE_NOT_USED											0x00
#define SPECIFIC_TYPE_POWER_SWITCH_MULTILEVEL			0x01
#define SPECIFIC_TYPE_MOTOR_MULTIPOSITION					0x03
#define SPECIFIC_TYPE_SCENE_SWITCH_MULTILEVEL			0x04
#define SPECIFIC_TYPE_CLASS_A_MOTOR_CONTROL				0x05
#define SPECIFIC_TYPE_CLASS_B_MOTOR_CONTROL				0x06
#define SPECIFIC_TYPE_CLASS_C_MOTOR_CONTROL				0x07
#define GENERIC_TYPE_SWITCH_REMOTE                      			0x12
#define GENERIC_TYPE_SWITCH_TOGGLE                      			0x13
#define GENERIC_TYPE_SENSOR_BINARY                      			0x20
#define GENERIC_TYPE_SENSOR_MULTILEVEL                  		0x21
#define GENERIC_TYPE_SENSOR_ALARM									0xa1
#define GENERIC_TYPE_WATER_CONTROL                      			0x22
#define GENERIC_TYPE_METER_PULSE                        				0x30
#define GENERIC_TYPE_ENTRY_CONTROL                      			0x40
#define GENERIC_TYPE_SEMI_INTEROPERABLE                 		0x50
#define GENERIC_TYPE_NON_INTEROPERABLE                  		0xFF
#define SPECIFIC_TYPE_ADV_ZENSOR_NET_SMOKE_SENSOR	0x0a
#define SPECIFIC_TYPE_BASIC_ROUTING_SMOKE_SENSOR		0x06
#define SPECIFIC_TYPE_BASIC_ZENSOR_NET_SMOKE_SENSOR	0x08
#define SPECIFIC_TYPE_ROUTING_SMOKE_SENSOR					0x07
#define SPECIFIC_TYPE_ZENSOR_NET_SMOKE_SENSOR			0x09

#define SENSOR_MULTILEVEL_GET											0x04

#define SENSOR_MULTILEVEL_REPORT_PRECISION_MASK		0xe0
#define SENSOR_MULTILEVEL_REPORT_SCALE_MASK				0x18
#define SENSOR_MULTILEVEL_REPORT_SIZE_MASK					0x07

#define SENSOR_MULTILEVEL_REPORT_SCALE_SHIFT				0x03
#define SENSOR_MULTILEVEL_REPORT_PRECISION_SHIFT		0x05

#define SENSOR_MULTILEVEL_REPORT_GENERAL_PURPOSE_VALUE	0x02
#define SENSOR_MULTILEVEL_REPORT_LUMINANCE					0x03
#define SENSOR_MULTILEVEL_REPORT_POWER							0x04
#define SENSOR_MULTILEVEL_REPORT_CO2_LEVEL					0x11
#define SENSOR_MULTILEVEL_REPORT_RELATIVE_HUMIDITY	0x05
#define SENSOR_MULTILEVEL_REPORT_TEMPERATURE				0x01

#define BASIC_REPORT																0x03
#define SENSOR_MULTILEVEL_REPORT										0x05

#define	SUCCESS																		0x00
#define	FAILURE																		0x01

#define	TRUE																			0x01
#define	FALSE																			0x00

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/file.h>

void initZWave(char *zwdevice);
void initZWNetwork();
char checksum(char *buff, int len);
void *receiveFunction(void); //Serial async receiver thread
void *decodeFrame(char *frame, int length);
unsigned char getCallbackID(void);
void sendFunction (char *buffer, size_t length, int type, int response);
void zwPopJob(void);
void addIntent(int nodeid, int type);
int getIntent(int type);
void handleCommandSensorMultilevelReport(int nodeid, int instance_id, int sensortype, int metadata, int val1, int val2, int val3, int val4);
void zwReadMemory(int offset, int len);
void zwWriteMemory(int offset, int len, unsigned char *data);


#endif
