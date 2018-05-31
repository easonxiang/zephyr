/****************************************************************************
 * examples/wget/wget_main.c
 *
 *   Copyright (C) 2009, 2011, 2015 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name Gregory Nutt nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>
#include <arch/sprd/sprd_sc2355x_internal.h>
#include <sched.h>

#include "engpc.h"
#include "eng_diag.h"

//just use 2048, phone use (4096*64)
#define DATA_BUF_SIZE (2048)

#define CONFIG_ENGPC_THREAD_DEFPRIO 100
#define CONFIG_ENGPC_THREAD_STACKSIZE 4096

typedef enum {
  ENG_DIAG_RECV_TO_AP,
  ENG_DIAG_RECV_TO_CP,
} eng_diag_state;

static unsigned  char log_data[128];
static char backup_data_buf[DATA_BUF_SIZE];
static int g_diag_status = ENG_DIAG_RECV_TO_AP;

/*paramters for poll thread*/
static char poll_name[] = "poll_thread";
static char poll_para[] = "poll thread from engpc";
static sem_t g_sem;

int eng_open_dev(char *dev, int mode)
{
	int fd;
	fd = open(dev, mode);
	if (fd < 0){
		printf("open device error path : %s\n", dev);
		return -1;
	}
	
	return fd;
}

int get_user_diag_buf( unsigned char *buf, int len) {
  int i;
  int is_find = 0;
  eng_dump(buf, len, len, 1, __FUNCTION__);

  for (i = 0; i < len; i++) {
    if (buf[i] == 0x7e && ext_buf_len == 0) {  // start
      ext_data_buf[ext_buf_len++] = buf[i];
    } else if (ext_buf_len > 0 && ext_buf_len < DATA_EXT_DIAG_SIZE) {
      ext_data_buf[ext_buf_len] = buf[i];
      ext_buf_len++;
      if (buf[i] == 0x7e) {
        is_find = 1;
        break;
      }
    }
  }
#if 0  // FOR DBEUG
    if ( is_find ) {
        for(i = 0; i < ext_buf_len; i++) {
            ENG_LOG("eng_vdiag 0x%x, ",ext_data_buf[i]);
        }
    }
#endif
  return is_find;
}

void init_user_diag_buf(void)
{
	memset(ext_data_buf, 0, DATA_EXT_DIAG_SIZE);
	ext_buf_len = 0;
}

int get_ser_diag_fd(void) { return serial_fd; }

int engpc_thread(int argc, FAR char *argv[])
{
	int has_processed = 0;
	int r_cnt;

	/*must set schedule policy to Round robin */
	struct sched_param param = {.sched_priority = 100};
	sched_setscheduler(getpid(), SCHED_RR, &param);

	serial_fd = eng_open_dev(SERIAL_DEV, O_RDWR);
	if (serial_fd < 0) {
		printf("open serial error\n");
		return -1;
	}

	printf("open serial success, fd : %d\n",serial_fd);
	init_user_diag_buf();

	while(1) {
		printf("start to read from serial\n");
		r_cnt = read(serial_fd, log_data, 128); //just read 128 byte.
		printf("%s read cnt : %d\n", __FUNCTION__, r_cnt);

		 if (get_user_diag_buf(log_data, r_cnt)) {
			g_diag_status = ENG_DIAG_RECV_TO_AP;
		 }
		 has_processed = eng_diag(ext_data_buf, ext_buf_len);
		 printf("ALL handle finished!!!!!!!!!\n");
		 memset(log_data,0x00, sizeof(log_data));
		 init_user_diag_buf();
	}

}

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int engpc_main(int argc, char *argv[])
#endif
{
	task_create("engpc", CONFIG_ENGPC_THREAD_DEFPRIO ,CONFIG_ENGPC_THREAD_STACKSIZE,
		(main_t)engpc_thread, (FAR char * const *)argv);
}

