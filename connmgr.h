#ifndef _CONNMGR_H_
#define _CONNMGR_H_

#include "sbuffer.h"

void connmgr_listen(int port_number, int max_connections, sbuffer_t *buffer);

void connmgr_free();

#endif /* _CONNMGR_H_ */