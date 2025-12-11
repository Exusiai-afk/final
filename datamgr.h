#ifndef DATAMGR_H
#define DATAMGR_H

#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "sbuffer.h"

void *datamgr_run(void *buffer);
void datamgr_free();

#endif // DATAMGR_H