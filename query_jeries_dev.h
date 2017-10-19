#ifndef QUERY_JERIES_DEV_H
#define QUERY_JERIES_DEV_H
#include <linux/ioctl.h>

#define DEVICE_NUM 'J'
#define DEVICE_NAME "jeries_driver"
/*Alert (Wake up) the device*/
#define JERIES_DEV_ALERT _IOR(DEVICE_NUM, 0, int)
/*Read State of the device (awake or asleep)*/
#define JERIES_DEV_GET_STATE _IOW(DEVICE_NUM, 1, int)
/*Put device to sleep*/
#define JERIES_DEV_SLEEP _IOR(DEVICE_NUM, 2, int)
#define JERIES_DEV_WRITE _IOR(DEVICE_NUM, 3, char)
#define JERIES_DEV_READ _IOWR(DEVICE_NUM, 4, char)

#endif
