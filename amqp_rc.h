#ifndef _AMQP_RC_H_
#define _AMQP_RC_H_

#include <pthread.h>
#include "camera-common.h"

void switch_device(CameraDevice* new_dev);
pthread_t* start_amqp_thread(void);
#endif
