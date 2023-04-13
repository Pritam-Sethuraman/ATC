#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/dispatch.h>

pthread_attr_t attr;
typedef struct _pulse msg_header_t;

#define OC_TO_CS "operator_console"

typedef struct OperatorMessage {
	msg_header_t hdr;
	int id;
	float speedX, speedY, speedZ;
}oc_msg_t;

typedef struct Timeframe {
	msg_header_t hdr;
	int n;
}time_msg_t;

void *threadUpdateSpeed(void *arg) {
	oc_msg_t speed_msg;
	int server_coid;

	if((server_coid = name_open(OC_TO_CS, 0)) == -1)
		pthread_exit(NULL);

	speed_msg.hdr.type = 0x00;
	speed_msg.hdr.subtype = 0x01;

	printf("\nEnter Flight Data: ");
	scanf("%d%f%f%f", &speed_msg.id, &speed_msg.speedX, &speed_msg.speedY, &speed_msg.speedZ);

	printf("Updating speed of Aircraft #%d\n", speed_msg.id);
	if(MsgSend(server_coid, &speed_msg, sizeof(speed_msg), NULL, 0) == -1)
		printf("Error sending message...\n");

	name_close(server_coid);
	pthread_exit(NULL);
}

void *threadUpdateTimeframe(void *arg) {
	time_msg_t time_msg;
	int server_coid;

	if((server_coid = name_open(OC_TO_CS, 0)) == -1)
		pthread_exit(NULL);

	time_msg.hdr.type = 0x00;
	time_msg.hdr.subtype = 0x02;

	scanf("\nEnter timeframe: %d", &time_msg.n);
	printf("Updating timeframe to %d seconds\n", &time_msg.n);
	if(MsgSend(server_coid, &time_msg, sizeof(time_msg), NULL, 0) == -1)
		printf("Error sending message...\n");

	name_close(server_coid);
	pthread_exit(NULL);
}

int main(int argc, char **argv) {
	pthread_t thread1;
	int rc;

	/* Initialize attributes */
	rc = pthread_attr_init(&attr);
	if (rc)
		printf("ERROR, RC from pthread_attr_init() is %d \n", rc);

	/* Set detach state */
	rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (rc)
		printf("ERROR; RC from pthread_attr_setdetachstate() is %d \n", rc);

	// Creating Threads
	rc = pthread_create(&thread1, NULL, threadUpdateSpeed, NULL);
	if (rc != 0) {
		printf("Error creating thread\n");
		return EXIT_FAILURE;
	}
	rc = pthread_create(&thread2, NULL, threadUpdateTimeframe, NULL);
	if (rc != 0) {
		printf("Error creating thread\n");
		return EXIT_FAILURE;
	}

	int ch;
	printf("\n----- Operator Console -----\n"
			"1. Update Speed\n"
			"2. Update timeframe\n");
	printf("Your choice: ");
	scanf("%d", &ch);
	if(ch == 1)
		pthread_join(thread1, NULL);
	else if(ch == 2)
		pthread_join(thread2, NULL);
	else
		printf("Invalid Option\n");

	return EXIT_SUCCESS;
}
