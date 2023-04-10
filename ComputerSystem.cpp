#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <sys/dispatch.h>

pthread_attr_t attr;
typedef struct _pulse msg_header_t;

#define R_TO_CS "radar"
#define OC_TO_CS "operator_console"
#define CS_TO_D "data_display"

// Airspace
#define MIN_X 0
#define MAX_X 100000
#define MIN_Y 0
#define MAX_Y 100000
#define MIN_Z 15000
#define MAX_Z 40000

// Constraints
#define DXY 3000
#define DZ 1000

// Structure to store data coming from Radar
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

typedef struct Timeframe {
	msg_header_t hdr;
	int n;
}time_msg_t;

time_msg_t timeframe;

// Server Thread for Client Radar
void *threadRadar(void *arg) {
	printf("[Computer System] Running server...\n\n");
	name_attach_t *attach;
	int rcvid;
	int server_coid;

	if ((server_coid = name_open(CS_TO_D, 0)) == -1) {
		pthread_exit(NULL);
	}

	if ((attach = name_attach(NULL, R_TO_CS, 0)) == NULL) {
		pthread_exit(NULL);
	}

	while (1) {
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

			//name_open() sends a connect message, must EOK this
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
					//printf("%f\t %d\t %f\t %f\t %f\t %f\t %f\t %f\n", aircraft_data[i].time, aircraft_data[i].id, aircraft_data[i].x, aircraft_data[i].y, aircraft_data[i].z, aircraft_data[i].speedX, aircraft_data[i].speedY, aircraft_data[i].speedZ);
				}
			}
			MsgReply(rcvid, EOK, 0, 0);
		}
	}
	/* Remove the name from the space */
	name_detach(attach, 0);
	pthread_exit(NULL);
}

// Thread to check for collision
void *threadCheckCollision(void *arg) {
	name_attach_t *attach;
	int rcvid;
	//aircraft_data_t aircraft_i_after_t[10], aircraft_j_after_t[10];

	if ((attach = name_attach(NULL, R_TO_CS, 0)) == NULL)
		pthread_exit(NULL);

	while (1) {
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

			//name_open() sends a connect message, must EOK this
			if (aircraft_data[i].hdr.type == _IO_CONNECT ) {
				MsgReply( rcvid, EOK, NULL, 0 );
				continue;
			}

			// Some other QNX IO message was received; reject it
			if (aircraft_data[i].hdr.type > _IO_BASE && aircraft_data[i].hdr.type <= _IO_MAX ) {
				MsgError( rcvid, ENOSYS );
				continue;
			}

			if (aircraft_data[i].hdr.type == 0x00) {
				if (aircraft_data[i].hdr.subtype == 0x01) {
					for (int j=i+1; j<10; j++) {
						// Check for minimum separation
						float distance = sqrt(pow(aircraft_data[i].x - aircraft_data[j].x, 2) + pow(aircraft_data[i].y - aircraft_data[j].y, 2) + pow(aircraft_data[i].z - aircraft_data[j].z, 2));
						float dxy = sqrt(pow(aircraft_data[i].x - aircraft_data[j].x, 2) + pow(aircraft_data[i].y - aircraft_data[j].y, 2));
						float dz = abs(aircraft_data[i].z - aircraft_data[j].z);
						if (distance < 50) {
							printf("\n!!! Collision Alert !!! \n");
							printf("Aircraft #%d and Aircraft #%d collided at T = %f seconds.\n", aircraft_data[i].id, aircraft_data[j].id, aircraft_data[i].time);

						}
						if (dxy < DXY && dz < DZ) {
							printf("\n!!! Safety Violation !!!\n");
							printf("Aircraft #%d and Aircraft #%d\n", aircraft_data[i].id, aircraft_data[j].id);
						}
						timeframe.n = 180;
						for (int t=0; t<timeframe.n; t++) {
							float x1 = aircraft_data[i].x + (aircraft_data[i].speedX * t);
							float y1 = aircraft_data[i].y + (aircraft_data[i].speedY * t);
							float z1 = aircraft_data[i].z + (aircraft_data[i].speedZ * t);

							float x2 = aircraft_data[j].x + (aircraft_data[j].speedX * t);
							float y2 = aircraft_data[j].y + (aircraft_data[j].speedY * t);
							float z2 = aircraft_data[j].z + (aircraft_data[j].speedZ * t);

							if (x1 == x2 && y1 == y2 && z1 == z2) {
								printf("\n!!! COLLISION DETECTED !!!\n");
								printf("Aircraft #%d and Aircraft #%d will be DEAD!\n", aircraft_data[i].id, aircraft_data[j].id);
								printf("Collision bound to happen in %f seconds\n", t);
							}
						}
					}
				}
			}
			MsgReply(rcvid, EOK, 0, 0);
		}
	}
	// Remove the name from the space
	name_detach(attach, 0);
	pthread_exit(NULL);
}

// Thread to send aircraft data to Display
void *threadDisplayAircraft(void *arg) {
	int server_coid;

	if ((server_coid = name_open(CS_TO_D, 0)) == -1) {
		pthread_exit(NULL);
	}

	for (int i=0; i<10; i++) {
		aircraft_data[i].hdr.type = 0x00;
		aircraft_data[i].hdr.subtype = 0x01;
	}

	while (1) {
		for(int i=0; i<10; i++) {
			if(MsgSend(server_coid, &aircraft_data[i], sizeof(aircraft_data[i]), NULL, 0) == -1)
				break;
			sleep(0.1);
		}
		sleep(1);
	}
	name_close(server_coid);
	pthread_exit(NULL);
}


// Thread to get speed and id from operator console
void *threadGetUpdateSpeed(void *arg) {
	name_attach_t *attach;
	oc_msg_t msg;
	int rcvid;

	if((attach = name_attach(NULL, OC_TO_CS, 0)) == NULL) {
		pthread_exit(NULL);
	}

	while(1) {
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

		if (msg.hdr.type == 0x00) {
			if (msg.hdr.subtype == 0x01) {
				printf("Msg received from OC...\n");
			}
		}
		MsgReply(rcvid, EOK, 0, 0);
	}
	name_detach(attach, 0);
	pthread_exit(NULL);
}


// Thread to post data to radar to update speed
void *threadPostUpdateSpeed(void *arg) {
	oc_msg_t msg;
	int server_coid;

	if((server_coid = name_open(R_TO_CS, 0)) == -1)
		pthread_exit(NULL);

	msg.hdr.type = 0x00;
	msg.hdr.subtype = 0x02;

	if(MsgSend(server_coid, &msg, sizeof(msg), NULL, 0) == -1)
		printf("Error sending message...\n");

	name_close(server_coid);
	pthread_exit(NULL);
}

// Thread to get timeframe from operator console
void *threadGetTimeframe(void *arg) {
	name_attach_t *attach;
	int rcvid;

	if((attach = name_attach(NULL, OC_TO_CS, 0)) == NULL) {
		pthread_exit(NULL);
	}

	while(1) {
		// Wait for a message on the connection
		rcvid = MsgReceive(attach->chid, &timeframe, sizeof(timeframe), NULL);

		// Check for errors
		if (rcvid == -1) {
			perror("MsgReceive Failed\n");
			break;
		}
		if (rcvid == 0) {
			switch (timeframe.hdr.code) {
			case _PULSE_CODE_DISCONNECT:
				ConnectDetach(timeframe.hdr.scoid);
			    break;
			case _PULSE_CODE_UNBLOCK:
				break;
			default:
				break;
			}
			continue;
		}

		/* name_open() sends a connect message, must EOK this */
		if (timeframe.hdr.type == _IO_CONNECT ) {
			MsgReply( rcvid, EOK, NULL, 0 );
		    continue;
		}

		/* Some other QNX IO message was received; reject it */
		if (timeframe.hdr.type > _IO_BASE && timeframe.hdr.type <= _IO_MAX ) {
			MsgError( rcvid, ENOSYS );
		    continue;
		}

		if (timeframe.hdr.type == 0x00) {
			if (timeframe.hdr.subtype == 0x02) {
				printf("Msg received from OC...\n");
			}
		}
		MsgReply(rcvid, EOK, 0, 0);
	}
	name_detach(attach, 0);
	pthread_exit(NULL);
}


// Main Thread
int main(int argc, char *argv[]) {
	// Creating threads for different clients
	pthread_t thread1, thread2, thread3, thread4, thread5, thread6;
	int rc;

	/* Initialize attributes */
	rc = pthread_attr_init(&attr);
	if (rc)
		printf("ERROR, RC from pthread_attr_init() is %d \n", rc);

	/* Set detach state */
	rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (rc)
		printf("ERROR; RC from pthread_attr_setdetachstate() is %d \n", rc);

	// Creating Thread1
	rc = pthread_create(&thread1, NULL, threadRadar, NULL);
	if (rc != 0) {
		printf("Error creating thread\n");
		return EXIT_FAILURE;
	}

	// Creating Thread2
	rc = pthread_create(&thread2, NULL, threadDisplayAircraft, NULL);
	if (rc != 0) {
		printf("Error creating thread\n");
		return EXIT_FAILURE;
	}

	// Creating Thread3
	rc = pthread_create(&thread3, NULL, threadCheckCollision, NULL);
	if (rc != 0) {
		printf("Error creating thread\n");
		return EXIT_FAILURE;
	}

	// Creating Thread4
	rc = pthread_create(&thread4, NULL, threadGetUpdateSpeed, NULL);
	if(rc != 0) {
		printf("Error creating thread\n");
		return EXIT_FAILURE;
	}

	// Creating Thread5
	rc = pthread_create(&thread5, NULL, threadPostUpdateSpeed, NULL);
	if(rc != 0) {
		printf("Error creating thread\n");
		return EXIT_FAILURE;
	}

	// Creating Thread6
	rc = pthread_create(&thread6, NULL, threadGetTimeframe, NULL);
	if(rc != 0) {
		printf("Error creating thread\n");
		return EXIT_FAILURE;
	}

	/* Synchronize all threads */
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	pthread_join(thread3, NULL);
	pthread_join(thread4, NULL);
	pthread_join(thread5, NULL);
	pthread_join(thread6, NULL);

	return EXIT_SUCCESS;
}
