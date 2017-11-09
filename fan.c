#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <unistd.h>

#define FALSE 0
#define TRUE 1
#define BUFFER_MAX 64

#define SERIAL_DEV_NAME		"/dev/ttyUSB0"
#define SERIAL_SPEED		57600

#define MODULE_ID 			0x5E
#define CMD_GET_SENSOR		0x05
#define CMD_FAN_ON 			0x0E
#define CMD_FAN_OFF 		0x0D
#define FAN_STATE_ON 		0x11
#define FAN_STATE_OFF 		0x12

#define STATUS_FAN_UNKNOWN 	0
#define STATUS_FAN_ON		1
#define STATUS_FAN_OFF		2

typedef struct _tx_packet_header
{
	unsigned char frame_type;
	unsigned char seq;
	unsigned char frame_dispatch;
	unsigned short dest_addr;
	unsigned short src_addr;
	unsigned char payload_length;
	unsigned char group_id;
	unsigned char type;
}__attribute__((packed)) tx_packet_header_t;

typedef struct _rx_packet_header
{
	unsigned char frame_type;
	unsigned char frame_dispatch;
	unsigned short dest_addr;
	unsigned short src_addr;
	unsigned char payload_length;
	unsigned char group_id;
	unsigned char type;
}__attribute__((packed)) rx_packet_header_t;

typedef struct _user_data
{
	unsigned char module_id;
	unsigned char cmd;
	unsigned char subcmd;
	unsigned char data_length;
}__attribute__((packed)) user_data_t;

typedef struct _ctrl_packet
{
	tx_packet_header_t packet_header;
	user_data_t user_data;
	unsigned short crc;	
}__attribute__((packed)) ctrl_packet_t;

typedef struct _ack_packet
{
	unsigned char frame_type;
	unsigned char frame_dispatch;
	unsigned short crc;	
}__attribute__((packed)) ack_packet_t;

typedef struct _sensor_data
{
	unsigned short cds;
	unsigned short temp;
	unsigned short humi;
	unsigned short light;	
	unsigned short bat;
}__attribute__((packed)) sensor_data_t;

typedef struct _shared_context
{
	float temp;
	float humi;
	unsigned fan_status;	
}__attribute__((packed)) shared_context_t;

unsigned char exit_flag = FALSE;
unsigned char ack_recved = FALSE;
shared_context_t shared_context = {0,};
int fd = -1;

// crc check function 
unsigned short crcByte(unsigned char *data, unsigned char Length)
{
	unsigned short crc = 0, i;
	for (i = 0; i < Length; i++)
	{
		crc = (unsigned char)(crc >> 8) | (crc << 8);
		crc ^= data[i];
		crc ^= (unsigned char)(crc & 0xff) >> 4;
		crc ^= crc << 12;
		crc ^= (crc & 0xff) << 5;
	}
	return crc;
}

// on packet receive 
void recv_packet_processing(unsigned char* packet, int size)
{
	unsigned char frame_type = 0;
	unsigned short packet_crc = 0, calc_crc = 0;
	rx_packet_header_t* ppacketheader = NULL;
	user_data_t* puserdata = NULL;
	sensor_data_t* psensor = NULL;
	unsigned char ext_data = 0;
	float temp,humi;

	packet_crc = *(unsigned short*)(packet+size-2);	//2 => .... + crc(2byte)
	calc_crc = crcByte(packet, size-2);

	if(packet_crc == calc_crc)
	{	
		frame_type = ((ack_packet_t*)packet)->frame_type;

		if(frame_type == 0x43)
		{
			//ack recevied 
			ack_recved = TRUE;				
		}
		else if(frame_type == 0x45)
		{
			ppacketheader = (rx_packet_header_t*)packet;
			puserdata =  (user_data_t*)(packet + sizeof(rx_packet_header_t));
						
			if(puserdata->module_id == MODULE_ID)
			{
				piLock(0);
				if(puserdata->cmd == 0x01 && puserdata->subcmd == 0x05 && puserdata->data_length == 0x0a)
				{
					psensor = (sensor_data_t*)(packet + sizeof(rx_packet_header_t) + sizeof(user_data_t));
					temp = psensor->temp;
          				temp = temp * 0.01 - 40;
          				humi = psensor->humi;
          				humi = temp * (0.01 + 0.00008 * humi) - 4.0 + 0.0405 * humi - 0.0000028 * humi * humi;

					
					shared_context.humi = humi;
					shared_context.temp = temp;		
					
				}
				else if(puserdata->cmd == 0x03)
				{
					if(puserdata->subcmd == FAN_STATE_ON)
						shared_context.fan_status = STATUS_FAN_ON;
					else if(puserdata->subcmd == FAN_STATE_OFF)
						shared_context.fan_status = STATUS_FAN_OFF;					
				} 
	
				piUnlock(0);
			}
		}
	}
}

// recv thread function 
void* recv_thread(void *data) 
{
	unsigned char prev_data = 0x7e;
	unsigned char read_data = 0;
	unsigned char data_idx = 0;
	unsigned char i = 0;
	
	unsigned char encode_detect = FALSE;
	unsigned char packet_valid = FALSE;
	
	unsigned char rx_data[BUFFER_MAX] = { 0, };
	unsigned char buffer[BUFFER_MAX] = { 0, };

	int readcount = 0;
			
	while (exit_flag == FALSE)
	{
		if ((readcount = serialDataAvail(fd)) > 0)
		{
			for (i = 0; i < readcount && i < BUFFER_MAX; i++)
				buffer[i] = serialGetchar(fd);
		}
		else{
			continue;
		}

		for (i = 0; i < readcount; i++)
		{
			read_data = buffer[i];

			if (prev_data == 0x7e && read_data == 0x7e)
			{
				//packet start detected
				memset(rx_data, sizeof(rx_data), 0);
				packet_valid = TRUE;
				data_idx = 0;								
			}
			else if (prev_data != 0x7e && read_data == 0x7e)
			{
				//packet end detected
				if (packet_valid == TRUE)
				{
					if (data_idx < sizeof(rx_data))
					{
						recv_packet_processing(rx_data, data_idx);
					}
					packet_valid = FALSE;
				}
			}
			else if (read_data == 0x7d && packet_valid == TRUE)
			{
				encode_detect = TRUE;	
			}
			else if (packet_valid == TRUE)
			{
				if(encode_detect == TRUE)
				{
					read_data ^= 0x20;
					encode_detect = FALSE;
				}

				if (data_idx < sizeof(rx_data))
				{
					rx_data[data_idx++] = read_data;					
				}
				else				
				{			
					packet_valid = FALSE;
				}				
			}			
			
			prev_data = read_data;
		}		
	}

	return 0;
}

unsigned char send_command(unsigned char cmd, unsigned char subcmd)
{
	int idx = 0, tx_idx = 0;
	int writeRsult, readRsult;
	unsigned char tx_data[BUFFER_MAX] = {0,};
	ctrl_packet_t packet = { 0, };
	unsigned char* ptemp = NULL;
	
	packet.packet_header.frame_type = 0x44;
	packet.packet_header.seq = 00;
	packet.packet_header.frame_dispatch = 0x00;
	packet.packet_header.dest_addr = 0xffff;
	packet.packet_header.src_addr = 0xffff;
	packet.packet_header.payload_length = sizeof(user_data_t);
	packet.packet_header.group_id = 00;
	packet.packet_header.type = 0x40;

	packet.user_data.module_id = MODULE_ID;
	packet.user_data.cmd = cmd;
	packet.user_data.subcmd = subcmd;
	packet.user_data.data_length = 0;

	packet.crc = crcByte((unsigned char*)&packet.packet_header, sizeof(tx_packet_header_t) + sizeof(user_data_t));
	
	tx_data[tx_idx++] = 0x7e;	//start
	ptemp = (unsigned char*)&packet;
	for(idx = 0; idx < sizeof(packet) && tx_idx < BUFFER_MAX-1; idx++)
	{
		if(ptemp[idx] == 0x7e || ptemp[idx] == 0x7d)
		{
			tx_data[tx_idx++] = 0x7d;
			tx_data[tx_idx++] = ptemp[idx]^0x20;
		}
		else
		{
			tx_data[tx_idx++] = ptemp[idx];
		}
	}	
	tx_data[tx_idx++]  = 0x7e; //end
	
	for (idx = 0; idx < tx_idx; idx++)
	{
		serialPutchar(fd, tx_data[idx]);
		delayMicroseconds(1);
	}
	
	return TRUE;
}


int exit()
{
    return 0;  
}


int init()
{
	int status = 0;
	
	float cur_temp = 0;
	float cur_humi = 0;
	unsigned char fan_status =  STATUS_FAN_UNKNOWN;

	if (wiringPiSetup() == -1)
	{
		fprintf(stderr, "Unable to start wiringPi: %s\n", strerror(errno));
	}

	if ((fd = serialOpen(SERIAL_DEV_NAME, SERIAL_SPEED)) < 0)
	{
		fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
	}

	if (piThreadCreate(recv_thread) < 0)
	{
		fprintf(stdout, "Unable to create thread: %s\n", strerror(errno));
	}

	send_command(0x02, CMD_FAN_ON);
	send_command(0x02, CMD_FAN_OFF);

	piLock(0);
	cur_temp = shared_context.temp;
	cur_humi = shared_context.humi;
	fan_status = shared_context.fan_status;
	piUnlock(0);

	return 0;
}


int controlFan(char* status_str, int status_code) 
{
	double cur_temp;
	double cur_humi;
	int fan_status;
	int result=0;
	int i=0;

	init();

	for(i=0; i<2; i++) {
		if(status_code == 0)
		    send_command(0x02, CMD_FAN_OFF);
		else if(status_code == 1)
		    send_command(0x02, CMD_FAN_ON);

		delay(300);

		send_command(0x01, CMD_GET_SENSOR);
		delay(100);	//wait 100ms

		piLock(0);
		sleep(1);
		cur_temp = shared_context.temp;
		cur_humi = shared_context.humi;
		fan_status = shared_context.fan_status;
		piUnlock(0);
		
		//printf("temperature : %0.2f , humidity :  %0.2f, ", cur_temp, cur_humi);
		//printf("C : %s\n", fan_status);

	}

	sprintf(status_str, "%0.2f,%0.2f,%d", cur_temp, cur_humi, fan_status);

	if(fan_status == 2 && status_code == 0) 
	    result = 0;
	else if(fan_status == 1 && status_code == 1) 
	    result = 0;

	return result;
}

