
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringSerial.h>


#define FALSE 0
#define TRUE 1
#define BUFFER_MAX 64

#define SERIAL_DEV_NAME		"/dev/ttyUSB0"
#define SERIAL_SPEED		57600

#define MODULE_ID 		0x33
#define CMD_GAS_OPEN 	0x17
#define CMD_GAS_CUT 	0x18

#define STATUS_GAS_UNKNOWN 0
#define STATUS_GAS_OPENED  1
#define STATUS_GAS_CLOSED  2

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

unsigned char exit_flag = FALSE;
unsigned char ack_recved = FALSE;
unsigned char gas_cut_status = STATUS_GAS_UNKNOWN;
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
	unsigned short packet_crc = 0, calc_crc = 0;

	packet_crc = *(unsigned short*)(packet + size - 2);	//2 => .... + crc(2byte)
	calc_crc = crcByte(packet, size - 2);

	if (packet_crc == calc_crc)
	{
		if (((ack_packet_t*)packet)->frame_type == 0x43)
		{
			//ack recevied
			ack_recved = TRUE;
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
				if (encode_detect == TRUE)
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
	unsigned char tx_data[BUFFER_MAX] = { 0, };
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
	for (idx = 0; idx < sizeof(packet) && tx_idx < BUFFER_MAX - 1; idx++)
	{
		if (ptemp[idx] == 0x7e || ptemp[idx] == 0x7d)
		{
			tx_data[tx_idx++] = 0x7d;
			tx_data[tx_idx++] = ptemp[idx] ^ 0x20;
		}
		else
		{
			tx_data[tx_idx++] = ptemp[idx];
		}
	}
	tx_data[tx_idx++] = 0x7e; //end


	for (idx = 0; idx < tx_idx; idx++)
	{
		serialPutchar(fd, tx_data[idx]);
		delayMicroseconds(1);
	}


	return TRUE;
}

void wait_10sec()
{
	int i = 0;

	printf("wait 10 sec : ");
	for (i = 0; i < 10; i++)
	{
		printf(".");
		fflush(stdout);
		delay(1000);
	}
	printf("\n");
}


int main()
{
	unsigned char user_cmd = 0;
	int status = 0;

	do
	{
		if (wiringPiSetup() == -1)
		{
			fprintf(stderr, "Unable to start wiringPi: %s\n", strerror(errno));
			break;
		}

		if ((fd = serialOpen(SERIAL_DEV_NAME, SERIAL_SPEED)) < 0)
		{
			fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
			break;
		}

		if (piThreadCreate(recv_thread) < 0)
		{
			fprintf(stdout, "Unable to create thread: %s\n", strerror(errno));
			break;
		}

		while (TRUE)
		{
			printf("input command (1=>open, 2=>close, q=>exit : ");
			while (TRUE)
			{
				user_cmd = getchar();
				if (user_cmd != '1' && user_cmd != '2' && user_cmd != 'q') continue;
				break;
			}

			switch (user_cmd)
			{
			case '1':
				if (gas_cut_status == STATUS_GAS_OPENED)
				{
					printf("Gas Circuit Breaker is already opened\n");
					break;
				}
				ack_recved = FALSE;
				send_command(0x02, CMD_GAS_OPEN);
				delay(100);
				if (ack_recved)
				{
					gas_cut_status = STATUS_GAS_OPENED;
					ack_recved = FALSE;
					wait_10sec();
				}
				break;
			case '2':
				if (gas_cut_status == STATUS_GAS_CLOSED)
				{
					printf("Gas Circuit Breaker is already closed\n");
					break;
				}
				ack_recved = FALSE;
				send_command(0x02, CMD_GAS_CUT);
				delay(100);
				if (ack_recved)
				{
					gas_cut_status = STATUS_GAS_CLOSED;
					ack_recved = FALSE;
					wait_10sec();
				}
				break;
			case 'q':
			case 'Q':
				exit_flag = TRUE;
				break;
			default:
				break;
			}

			if (exit_flag == TRUE)
				break;
		}

	} while (0);

	if (fd >= 0)
		serialClose(fd);

	return 0;
}

int gas_open() 
{
	unsigned char user_cmd = 0;
	int status = 0;

	if (wiringPiSetup() == -1)
	{
		fprintf(stderr, "Unable to start wiringPi: %s\n", strerror(errno));
		return -1;
	}

	if ((fd = serialOpen(SERIAL_DEV_NAME, SERIAL_SPEED)) < 0)
	{
		fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
		return -1;
	}

	if (piThreadCreate(recv_thread) < 0)
	{
		fprintf(stdout, "Unable to create thread: %s\n", strerror(errno));
		return -1;
	}

	ack_recved = FALSE;
	send_command(0x02, CMD_GAS_OPEN);
	delay(100);
	if (ack_recved)
	{
		gas_cut_status = STATUS_GAS_OPENED;
		ack_recved = FALSE;
		wait_10sec();
	}

	return 0;
}


int gas_close() 
{
	unsigned char user_cmd = 0;
	int status = 0;

	if (wiringPiSetup() == -1)
	{
		fprintf(stderr, "Unable to start wiringPi: %s\n", strerror(errno));
		return -1;
	}

	if ((fd = serialOpen(SERIAL_DEV_NAME, SERIAL_SPEED)) < 0)
	{
		fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
		return -1;
	}

	if (piThreadCreate(recv_thread) < 0)
	{
		fprintf(stdout, "Unable to create thread: %s\n", strerror(errno));
		return -1;
	}

	ack_recved = FALSE;
	send_command(0x02, CMD_GAS_CUT);
	delay(100);
	if (ack_recved)
	{
		gas_cut_status = STATUS_GAS_CLOSED;
		ack_recved = FALSE;
		wait_10sec();
	}

	return 0;
}
