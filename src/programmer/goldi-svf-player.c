#include <libxsvf.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "../logging/log.h"
#include "goldi-programmer.h"
#include "bcmGPIO.h"

struct udata_s {
	FILE *f;
	int verbose;
	int clockcount;
	int bitcount_tdi;
	int bitcount_tdo;
	int retval_i;
	int retval[256];
};

static int h_setup(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 2) {
		fprintf(stderr, "[SETUP]\n");
		fflush(stderr);
	}
	initGPIO();
	return 0;
}

static int h_shutdown(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 2) {
		fprintf(stderr, "[SHUTDOWN]\n");
		fflush(stderr);
	}
	stopGPIO();
	return 0;
}

static void h_udelay(struct libxsvf_host *h, long usecs, int tms, long num_tck)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 3) {
		fprintf(stderr, "[DELAY:%ld, TMS:%d, NUM_TCK:%ld]\n", usecs, tms, num_tck);
		fflush(stderr);
	}
	if (num_tck > 0) {
		struct timeval tv1, tv2;
		gettimeofday(&tv1, NULL);
		writeGPIO(TMS, tms);
		while (num_tck > 0) {
			writeGPIO(TCK, 0);
			writeGPIO(TCK, 1);
			num_tck--;
			//printf("[TMS:%d, TDI:%d, TDO_LINE:%d]\n", tms, 0, readGPIO(TDO));
		}
		gettimeofday(&tv2, NULL);
		if (tv2.tv_sec > tv1.tv_sec) {
			usecs -= (1000000 - tv1.tv_usec) + (tv2.tv_sec - tv1.tv_sec - 1) * 1000000;
			tv1.tv_usec = 0;
		}
		usecs -= tv2.tv_usec - tv1.tv_usec;
		if (u->verbose >= 3) {
			fprintf(stderr, "[DELAY_AFTER_TCK:%ld]\n", usecs > 0 ? usecs : 0);
			fflush(stderr);
		}
	}
	if (usecs > 0) {
		usleep(usecs);
	}
}

static int h_getbyte(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	return fgetc(u->f);
}

static int h_pulse_tck(struct libxsvf_host *h, int tms, int tdi, int tdo, int rmask, int sync)
{
	struct udata_s *u = h->user_data;

	writeGPIO(TMS, tms);

	if (tdi >= 0) {
		u->bitcount_tdi++;
		writeGPIO(TDI, tdi);
	}

	writeGPIO(TCK, 0);
	writeGPIO(TCK, 1);

	int line_tdo = readGPIO(TDO);
	int rc = line_tdo >= 0 ? line_tdo : 0;

	if (rmask == 1 && u->retval_i < 256)
		u->retval[u->retval_i++] = line_tdo;

	if (tdo >= 0 && line_tdo >= 0) {
		u->bitcount_tdo++;
		if (tdo != line_tdo)
			rc = -1;
	}

	//printf("[TMS:%d, TDI:%d, TDO_ARG:%d, TDO_LINE:%d, RMASK:%d, RC:%d]\n", tms, tdi, tdo, line_tdo, rmask, rc);
	//printf("[TMS:%d, TDI:%d, TDO_LINE:%d]\n", tms, tdi, line_tdo);

	u->clockcount++;
	return rc;
}

static int h_set_frequency(struct libxsvf_host *h, int v)
{
	fprintf(stderr, "WARNING: Setting JTAG clock frequency to %d ignored!\n", v);
	return 0;
}

static void h_report_error(struct libxsvf_host *h, const char *file, int line, const char *message)
{
	fprintf(stderr, "[%s:%d] %s\n", file, line, message);
}

static int realloc_maxsize[LIBXSVF_MEM_NUM];

static void *h_realloc(struct libxsvf_host *h, void *ptr, int size, enum libxsvf_mem which)
{
	struct udata_s *u = h->user_data;
	if (size > realloc_maxsize[which])
		realloc_maxsize[which] = size;
	if (u->verbose >= 3) {
		fprintf(stderr, "[REALLOC:%s:%d]\n", libxsvf_mem2str(which), size);
	}
	return realloc(ptr, size);
}

static struct udata_s u;

static struct libxsvf_host h = {
	.udelay = h_udelay,
	.setup = h_setup,
	.shutdown = h_shutdown,
	.getbyte = h_getbyte,
	.pulse_tck = h_pulse_tck,
	.pulse_sck = NULL,
	.set_trst = NULL,
	.set_frequency = h_set_frequency,
	.report_tapstate = NULL,
	.report_device = NULL,
	.report_status = NULL,
	.report_error = h_report_error,
	.realloc = h_realloc,
	.user_data = &u
};

int programFPGA(char* filepath)
{
	if (!bcm2835_init())
	{
		log_error("gpio interface could not be started");
		return 1;
	}
		
	u.f = fopen(filepath, "rb");
	if (u.f == NULL) {
		log_error("programming file could not be read");
		return 1;
	}
	if(libxsvf_play(&h, LIBXSVF_MODE_SVF) < 0) {
		log_error("error occurred during programming");
		return 1;
	}

	bcm2835_close();
	
	return 0;
}

int programControlUnitFPGA(char* filepath)
{
	//TODO extra changes to ensure main FPGA doesn't get programmed
	u.f = fopen(filepath, "rb");
	if (u.f == NULL) {
		return 1;
	}
	if(libxsvf_play(&h, LIBXSVF_MODE_SVF) < 0) {
		return 1;
	}
	
	return 0;
}