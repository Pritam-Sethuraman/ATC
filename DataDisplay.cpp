#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>
using namespace std;

#define CS_TO_D "data_display"

// Airspace
#define MIN_X 0
#define MAX_X 100000
#define MIN_Y 0
#define MAX_Y 100000
#define MIN_Z 15000
#define MAX_Z 40000

pthread_attr_t attr;
typedef struct _pulse msg_header_t;

typedef struct Aircraft {
	msg_header_t hdr;
	float time;
	int id;
	float x, y, z;
	float speedX, speedY, speedZ;
}aircraft_data_t;

aircraft_data_t aircraft_data[10];

void *threadDisplayAircrafts(void *arg) {
	printf("[Data Display] Running server...\n\n");
	name_attach_t *attach;
	int rcvid;

	if ((attach = name_attach(NULL, CS_TO_D, 0)) == NULL)
		pthread_exit(NULL);

	while (1) {
		printf("\nTime\t Aircraft ID\t X\t Y\t Z\t SpeedX\t SpeedY\t SpeedZ\n");
		for (int i=0; i<10; i++) {
			rcvid = MsgReceive(attach->chid, &aircraft_data[i], sizeof(aircraft_data[i]), NULL);
			// Error condition, exit
			if (rcvid == -1)
				break;

			// Pulse received
			if (rcvid == 0) {
				switch (aircraft_data[i].hdr.code) {
				case _PULSE_CODE_DISCONNECT:
					ConnectDetach(aircraft_data[i].hdr.scoid);
					break;
				case _PULSE_CODE_UNBLOCK:
					break;
				default:
					break;
				}
				continue;
			}

			// name_open() sends a connect message, must EOK this
			if (aircraft_data[i].hdr.type == _IO_CONNECT ) {
				MsgReply( rcvid, EOK, NULL, 0 );
				continue;
			}

			// Some other QNX IO message was received; reject it
			if (aircraft_data[i].hdr.type > _IO_BASE && aircraft_data[i].hdr.type <= _IO_MAX ) {
				MsgError( rcvid, ENOSYS );
				continue;
			}

			// Getting Messages from Client
			if (aircraft_data[i].hdr.type == 0x00) {
				if (aircraft_data[i].hdr.subtype == 0x01) {
					if(aircraft_data[i].x >= MIN_X && aircraft_data[i].x <= MAX_X && aircraft_data[i].y >= MIN_Y && aircraft_data[i].y <= MAX_Y && aircraft_data[i].z >= MIN_Z && aircraft_data[i].z <= MAX_Z)
						printf("%f\t %d\t %f\t %f\t %f\t %f\t %f\t %f\n", aircraft_data[i].time, aircraft_data[i].id, aircraft_data[i].x, aircraft_data[i].y, aircraft_data[i].z, aircraft_data[i].speedX, aircraft_data[i].speedY, aircraft_data[i].speedZ);
					else
						printf("!!! Aircraft #%d is out of range. !!!\n", aircraft_data[i].id);
				}
			}
			MsgReply(rcvid, EOK, 0, 0);
		}
	}
	// Remove the name from the space
	name_detach(attach, 0);
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	pthread_t thread;
	int rc;

	// Initialize attributes
	rc = pthread_attr_init(&attr);
	if (rc)
		printf("ERROR, RC from pthread_attr_init() is %d \n", rc);

	// Set detach state
	rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (rc)
		printf("ERROR; RC from pthread_attr_setdetachstate() is %d \n", rc);

	// Creating Thread
	rc = pthread_create(&thread, NULL, threadDisplayAircrafts, NULL);
	if (rc != 0) {
		printf("Error creating thread\n");
		return EXIT_FAILURE;
	}

	// Synchronize all threads
	rc = pthread_join(thread, NULL);
	if (rc != 0) {
		printf("Error joining thread\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
