#include <arpa/inet.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ftputil.h"


/** Takes a string and modifies it such that there is no leading or trailing
 * whitespace.
 *
 * \param '*str' the input string which will be modified.
 * \return void.
 */
void trim(char *str) {
	int i;
	int len = strlen(str);
	int begin = 0;
	int end = len - 1;

	/* Find index of first non-whitespace character */
	while ((begin < len) && isspace((unsigned char) str[begin]))
		begin++;

	/* Find index of last non-whitespace character */
	while ((end >= begin) && isspace((unsigned char) str[end]))
		end--;

	/* Shift all characters caught between leading and trailing whitespace
	 * to the start of the string */
	for (i = begin; i <= end; i++)
		str[i - begin] = str[i];

	/* Note: if the string contained nothing but whitespace, a string of 0
	 * length is returned */
	str[i - begin] = '\0';
}


/* Modifies 'port' to contain the port of the socket represented by 'fd'.
 *
 * \param 'fd' a file descriptor representing a socket.
 * \param '*port' a pointer to a short which will have its value modified to
 *     the port of the socket.
 * \return 0 upon success, a negative int upon failure.
 */
int get_port(int fd, uint16_t *port){
	struct sockaddr_in s;
	socklen_t len = sizeof(s);

	if (getsockname(fd, (struct sockaddr *)&s, &len) == -1) {
		return -1;
	} else {
		(*port) = ntohs(s.sin_port);
		return 0;
	}
}


// TODO: what does this function do? My best guess at the moment is that it
// modifies 'ip', and 'port' to contain the IP address and port of the socket
// represented by 'fd'
int get_ip_port(int fd, char *ip, uint16_t *port){
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);

	getsockname(fd, (struct sockaddr *) &addr, &len);
	// TODO: potential security risk. Switch to strncpy?
	sprintf(ip, inet_ntoa(addr.sin_addr));
	*port = (uint16_t) ntohs(addr.sin_port);
	return 1;
}


void close_data_connections(int *datafds) {
	for (int i = 0; i < NDATAFD; i++) {
		close(datafds[i]);
	}
}
