#ifndef __ENGPC_H__
#define __ENGPC_H__

#include <termios.h>

#define SERIAL_DEV "/dev/ttyS1"
#define ENG_LOG(fmt,...) printf(fmt, __VA_ARGS__)
#define DATA_EXT_DIAG_SIZE (128)

char ext_data_buf[DATA_EXT_DIAG_SIZE];
int ext_buf_len;
int serial_fd;
int get_ser_diag_fd(void);

#endif
