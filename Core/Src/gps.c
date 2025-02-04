/*
*   Control code for the GPS
*   Contains update task, callback handler, and init function
*   GPS specific functionality is in lib/MTK3339/
*   This file handles the UART and passing the data off
*
*   RIT Launch Initiative
*/
#include "gps.h"
#include "usart.h"
#include "lib/common/common.h"
#include "lib/ringbuff/ringbuff.h"
#include "lib/TinyScheduler/ts.h"
#include "lib/MTK3339/MTK.h"
#include "log.h"
#include <stdint.h>

#define GPS_UART huart2
#define GPS_STARTL '$'
#define GPS_ENDL '\n'
#define GPS_BUFFER_SIZE 256 // should be 2 x maximum packet length


// task to be scheduled
tiny_task_t gps_task;

// location of resultant data
gga_packet_t gps_packet;

// local data
uint8_t gps_char;
// 'lines' needs to be fetched from memory every read, updated by both task loop and ISR
// after looking at the assembly it reloads it anyways (using ldrb), but best to change in case the compiler changes
volatile unsigned char lines = 0;
uint8_t rb_buff[GPS_BUFFER_SIZE];
ringbuff_t rb;


void gps_update(tiny_task_t* task);

RetType gps_init() {
    rbuff_init(&rb, rb_buff, GPS_BUFFER_SIZE);
    gps_task.start_time = ts_systime();
    gps_task.default_priority = SLEEP_PRIORITY;
    gps_task.task = &gps_update;

    init_gga(NMEA_OUTPUT_DEFAULT, NMEA_RATE_DEFAULT);

    // this has to be the last thing init does
    if(HAL_OK != HAL_UART_Receive_IT(&GPS_UART, &gps_char, 1)) {
        #ifdef DEBUG
        printf("gps init failed!\r\n");
        #endif

        return RET_ERROR;
    }
    return RET_OK;
}

// TODO check memcpyout errors (returns an error code, currently not checking it)
void gps_update(tiny_task_t* task) {
    disable_irq;
    lines--; // event is being handled
    enable_irq;

    uint8_t buff[GPS_BUFFER_SIZE / 2];
    uint8_t c = '\0';
    while(c != GPS_STARTL) {
        rbuff_memcpyout(&c, &rb, 1);
    }

    size_t i = 0;
    while(c != GPS_ENDL && i < (GPS_BUFFER_SIZE / 2)) { // maybe remove sanity check for size
        buff[i] = c;
        rbuff_memcpyout(&c, &rb, 1);
        i++;
    }

    buff[i] = '\0';
    parse_gga((char*)buff, &gps_packet, i);

    // copy the data out to the logger
    float lat_mul = 1.0;
    if(gps_packet.latitude.degrees < 0) {
        lat_mul = -1.0;
    }
    float long_mul = 1.0;
    if(gps_packet.longitude.degrees < 0) {
        long_mul = -1.0;
    }
    log_packet.latitude = ((float)gps_packet.latitude.degrees) + (gps_packet.latitude.minutes / 60 * lat_mul);
    log_packet.longitude = ((float)gps_packet.longitude.degrees) + (gps_packet.longitude.minutes / 60 * long_mul);
    log_packet.gps_alt = gps_packet.altitude;
    log_packet.gps_time = gps_packet.time;
    log_packet.gps_fix = gps_packet.fix;
    log_packet.gps_sats = gps_packet.sat_count;


    #ifdef DEBUG
    printf("%s\n", buff);

    printf("time: %i, lat: %io%f\", long: %io%f\", alt: %f, fix: %i, sats: %i\r\n\r\n", gps_packet.time,
            gps_packet.latitude.degrees, gps_packet.latitude.minutes, gps_packet.longitude.degrees,
            gps_packet.longitude.minutes, gps_packet.altitude, gps_packet.fix, gps_packet.sat_count);
    #endif

    if(!lines) { // check if we have more lines to process
        task->priority = SLEEP_PRIORITY; // we can sleep the task
    }
}

// TODO check for errors on return with memcpyin
// we can assume interrupts are disabled (TODO double check this condition is true)
void gps_RxCallback() {
    rbuff_memcpyin(&rb, &gps_char, 1);
    if(gps_char == GPS_ENDL) {
        lines++; // we have another line to handle!
        gps_task.priority = LOW_PRIORITY; // TODO maybe make this dynamic
        gps_task.start_time = ts_systime(); // this may be unnecessary
    }

    HAL_UART_Receive_IT(&GPS_UART, &gps_char, 1);
}
