#include "common.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

__attribute__((constructor))
static void ___init__(void)
{
	char *p = getenv(AFTER_LOAD_ENV);
	if (!p) {
		return;
	}

	int fd = atoi(p);
	unsetenv(AFTER_LOAD_ENV);
	unsetenv("LD_PRELOAD");

	/* Tell with-overlayfs that it can do the umounts */
	char q[1];
	write(fd, q, 1);

	/* Wait for ack */
	read(fd, q, 1);
	close(fd);
}
