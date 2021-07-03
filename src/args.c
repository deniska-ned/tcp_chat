#include "args.h"

#include "error_codes.h"


#define ARGS_PORT_DEF 2021

int parse_args(struct args *args)
{
    args->port = ARGS_PORT_DEF;
    
    return ERR_OKAY;
}
