#ifndef ARGS_H
#define ARGS_H

#include <inttypes.h>

struct args
{
    uint16_t port;
};


int parse_args(struct args *args);

#endif // ARGS_H
