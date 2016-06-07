#ifndef _TCPSERVER_H_
#define _TCPSERVER_H_

#include "typedef.h"

void connection_accept(struct evconnlistener *listener,
	evutil_socket_t fd, struct sockaddr *address, int socklen,
	void *ctx);

void connection_accept_error(struct evconnlistener *listener, void *ctx);

#endif
