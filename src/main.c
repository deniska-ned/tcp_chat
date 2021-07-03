#include "args.h"
#include "error_codes.h"
#include "server.h"


int main(int argc, char **argv)
{
    int rc = ERR_OKAY;

    struct args args;

    if (ERR_OKAY != (rc = parse_args(&args)))
        return rc;

    struct server server = {
        .master_port = args.port,
        .master_socket = -1
    };

    if (ERR_OKAY != (rc = server_prepare(&server)))
        return rc;

    if (ERR_OKAY != (rc = server_run(&server)))
        return rc;

    return rc;
}
