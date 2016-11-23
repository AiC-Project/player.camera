#ifndef _RC_H_
#define _RC_H_

#include <pthread.h>
#include "camera-common.h"

int connect_socket(const char* ip, short port);
void switch_device(CameraDevice* new_dev);
pthread_t* start_amqp_thread(void);
pthread_t* start_socket_thread(void);
#endif
