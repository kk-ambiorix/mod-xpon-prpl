// System headers
#include <stdio.h>
#include <string.h> // strlen()
#include <libgen.h> /* basename() */

// Own headers
#include "dbg_commands.h" /* ADD_INSTANCE */
#include "debugclnt.h"

static const char* const SERVER = "/var/run/onu_hal_dbg.sock";

static void usage(const char* name) {
    printf("Usage:\n");
    printf("%s -h\n", name);
    printf("%s <command>\n", name);
    printf("\n");
    printf("Options:\n");
    printf("  %-22s : show this help and exit\n", "-h");
    printf("\n");
    printf("Commands:\n");
    printf("  %-22s : add transceiver.2 and send dm:instance-added notification\n", ADD_INSTANCE);
    printf("  %-22s : remove transceiver.2 and send dm:instance-removed notification\n", REMOVE_INSTANCE);
    printf("  %-22s : change transceiver.1 and send dm:object-changed notification\n", CHANGE_TRANSCEIVER);
    printf("  %-22s : change onu_activation and send dm:object-changed notification\n", CHANGE_ONU_ACTIVATION);
    printf("  %-22s : send omci:reset_mib notification\n", OMCI_RESET_MIB);
}

static void handle_command(const char* cmd) {
    char buf[32];
    snprintf(buf, 32, "%s", cmd);
    dbg_handle_command(SERVER, buf, strlen(buf));
}

int main(int argc, char* argv[]) {
    const char* program = basename(argv[0]);
    if(argc <= 1) {
        usage(program);
        return -1;
    }
    const char* command = argv[1];

    if((strcmp(command, "-h") == 0) ||
       (strcmp(command, "--help") == 0)) {
        usage(program);
        return 0;
    }
    if((strcmp(command, ADD_INSTANCE) == 0) ||
       (strcmp(command, REMOVE_INSTANCE) == 0) ||
       (strcmp(command, CHANGE_TRANSCEIVER) == 0) ||
       (strcmp(command, CHANGE_ONU_ACTIVATION) == 0) ||
       (strcmp(command, OMCI_RESET_MIB) == 0)) {
        handle_command(command);
    } else {
        printf("%s: unknown command\n", command);
        return -1;
    }

    return 0;
}
