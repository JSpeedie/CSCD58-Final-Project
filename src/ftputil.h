#include <arpa/inet.h>
#include <stdint.h>

#define MAXLINE 4096
#define LISTENQ 1024
#define NDATAFD 4
#define TRUE 1
#define FALSE 0
#define DEBUG_OUTPUT 0
#define CMD_LS 1
#define CMD_GET 2
#define CMD_PUT 3
#define CMD_QUIT 4

#define FD_SETS(fds, rdset, n, i)\
  do {\
    for (i = 0; i < n; i++) { FD_SET(fds[i], rdset); }\
  } while (0)

void trim(char *str);
int get_port(int fd, uint16_t *port);
int get_ip_port(int fd, char *ip, uint16_t *port);
void close_data_connections(int *datafds);
