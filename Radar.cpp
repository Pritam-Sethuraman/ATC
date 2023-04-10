#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <fstream>
#include <sstream>
#include <pthread.h>
#include <sys/dispatch.h>
using namespace std;

pthread_attr_t attr;
typedef struct _pulse msg_header_t;

#define R_TO_CS "radar"

// Airspace
#define MIN_X 0
#define MAX_X 100000
#define MIN_Y 0
#define MAX_Y 100000
#define MIN_Z 15000
#define MAX_Z 40000

typedef struct Aircraft {
	msg_header_t hdr;
	float time;
	int id;
	float x, y, z;
	float speedX, speedY, speedZ;
}aircraft_data_t;

aircraft_data_t aircraft_data[10];

typedef struct OperatorMessage {
	msg_header_t hdr;
	int id;
	float speedX, speedY, speedZ;
}oc_msg_t;

// Thread to POST Aircraft Data to Computer System
void *threadClient(void *arg) {
	printf("[Radar] Client Running...\n\n");
	int server_coid; //server connection ID.

	if ((server_coid = name_open(R_TO_CS, 0)) == -1) {
		pthread_exit(NULL);
	}

	for (int i=0; i<10; i++) {
		aircraft_data[i].hdr.type = 0x00;
		aircraft_data[i].hdr.subtype = 0x01;
	}

	// File pointer
	const char* filename = "inputData.csv";
	ifstream file(filename);
	string line;
	if (file.is_open()) {
		int i=0;
		while (getline(file, line) && i < 10) {
			stringstream ss(line);
			string token;
			int j=0;
			while (getline(ss, token, ',')) {
				switch (j) {
				case 0:
					aircraft_data[i].time = stof(token);
					break;
				case 1:
					aircraft_data[i].id = stoi(token);
					break;
				case 2:
					aircraft_data[i].x = stof(token);
					break;
				case 3:
					aircraft_data[i].y = stof(token);
					break;
				case 4:
					aircraft_data[i].z = stof(token);
					break;
				case 5:
					aircraft_data[i].speedX = stof(token);
					break;
				case 6:
					aircraft_data[i].speedY = stof(token);
					break;
				case 7:
					aircraft_data[i].speedZ = stof(token);
					break;
				default:
					printf("Error: Too many columns in input file\n");
				}
				j++;
			}
			if(ss.eof())
				i++;
			else
				printf("Error: Too few columns in input file\n");
		}
		file.close();
	}
	else
		printf("Error: Unable to open input file\n");


	/* Do whatever work you wanted with server connection */

	while (1) {
		// Get the current time using the clock_gettime function
		struct timespec start_time;
		clock_gettime(CLOCK_REALTIME, &start_time);
		printf("\n[Radar] Sending Aircraft Data...\n");
		//printf("Time\t Aircraft ID\t X\t Y\t Z\t SpeedX\t SpeedY\t SpeedZ\n");
		for(int i=0; i<10; i++) {
			//printf("%f\t %d\t %f\t %f\t %f\t %f\t %f\t %f\n", aircraft_data[i].time, aircraft_data[i].id, aircraft_data[i].x, aircraft_data[i].y, aircraft_data[i].z, aircraft_data[i].speedX, aircraft_data[i].speedY, aircraft_data[i].speedZ);
			if (MsgSend(server_coid, &aircraft_data[i], sizeof(aircraft_data[i]), NULL, 0) == -1)
				break;
			// sleep for 0.1 second before sending next aircraft data
			sleep(0.1);
		}
		sleep(1);

		// Calculating time elapsed
		struct timespec end_time;
		clock_gettime(CLOCK_REALTIME, &end_time);
		long long duration = (end_time.tv_sec - start_time.tv_sec)*1000000000 + (end_time.tv_nsec - start_time.tv_nsec);
		float elapsed_time = duration / 1000000000;

		// Updating position of aircraft using speed
		for (int i=0; i<10; i++) {
			aircraft_data[i].time = aircraft_data[i].time  + elapsed_time;
			aircraft_data[i].x += aircraft_data[i].speedX * elapsed_time;
			aircraft_data[i].y += aircraft_data[i].speedY * elapsed_time;
			aircraft_data[i].z += aircraft_data[i].speedZ * elapsed_time;
		}
	}
	/* Close the connection */
	name_close(server_coid);
	pthread_exit(NULL);
}

// Thread to update speed of aircraft
void *threadUpdateSpeed(void *arg) {
	name_attach_t *attach;
	oc_msg_t msg;
	int rcvid;

	if ((attach = name_attach(NULL, R_TO_CS, 0)) == NULL) {
		pthread_exit(NULL);
	}

	while (1) {
		// Wait for a message on the connection
		rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);

		// Check for errors
		if (rcvid == -1) {
			perror("MsgReceive Failed\n");
			break;
		}
		if (rcvid == 0) {
			switch (msg.hdr.code) {
			case _PULSE_CODE_DISCONNECT:
				ConnectDetach(msg.hdr.scoid);
			    break;
			case _PULSE_CODE_UNBLOCK:
				break;
			default:
				break;
			}
			continue;
		}

		/* name_open() sends a connect message, must EOK this */
		if (msg.hdr.type == _IO_CONNECT ) {
			MsgReply( rcvid, EOK, NULL, 0 );
		    continue;
		}

		/* Some other QNX IO message was received; reject it */
		if (msg.hdr.type > _IO_BASE && msg.hdr.type <= _IO_MAX ) {
			MsgError( rcvid, ENOSYS );
		    continue;
		}

		// Processing the msg received
		if (msg.hdr.type == 0x00) {
			if (msg.hdr.subtype == 0x02) {
				for (int i=0; i<10; i++) {
					if (aircraft_data[i].id == msg.id) {
						aircraft_data[i].speedX = msg.speedX;
						aircraft_data[i].speedY = msg.speedY;
						aircraft_data[i].speedZ = msg.speedZ;
					}
				}
			}
		}
		MsgReply(rcvid, EOK, 0, 0);
	}

	name_detach(attach, 0);
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	pthread_t thread1, thread2;
	int rc;

	/* Initialize attributes */
	rc = pthread_attr_init(&attr);
	if (rc)
		printf("ERROR, RC from pthread_attr_init() is %d \n", rc);

	/* Set detach state */
	rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (rc)
		printf("ERROR; RC from pthread_attr_setdetachstate() is %d \n", rc);

	// Creating Thread
	rc = pthread_create(&thread1, NULL, threadClient, NULL);
	if (rc != 0) {
		printf("Error creating thread\n");
		return EXIT_FAILURE;
	}

	// Creating Thread
	rc = pthread_create(&thread2, NULL, threadUpdateSpeed, NULL);
	if (rc != 0) {
		printf("Error creating thread\n");
		return EXIT_FAILURE;
	}

	/* Synchronize all threads */
	rc = pthread_join(thread1, NULL);
	rc = pthread_join(thread2, NULL);

	return EXIT_SUCCESS;
}
