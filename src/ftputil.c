#include <unistd.h>
#include "ftputil.h"

void close_data_connections(int *datafds) {
    int i;
    for (i=0; i < NDATAFD; i++) {
        close(datafds[i]);
    }
}
