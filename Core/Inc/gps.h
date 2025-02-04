#ifndef GPS_H
#define GPS_H

#include "lib/TinyScheduler/ts.h"
#include "lib/MTK3339/MTK.h"
#include "lib/common/common.h"

// call this before using the GPS
// returns RET_OK on success, RET_ERROR on failure
RetType gps_init();

// call this in HAL_UART_RxCpltCallback for whatever uart the GPS is connected to
void gps_RxCallback();

// this is where the parsed gps data gets placed
// TODO put this somewhere else?
extern gga_packet_t gps_packet;

// this is the task that the scheduler needs
extern tiny_task_t gps_task;

#endif
