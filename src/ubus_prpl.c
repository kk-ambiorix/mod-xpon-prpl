/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2022 SoftAtHome
**
** Redistribution and use in source and binary forms, with or
** without modification, are permitted provided that the following
** conditions are met:
**
** 1. Redistributions of source code must retain the above copyright
** notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above
** copyright notice, this list of conditions and the following
** disclaimer in the documentation and/or other materials provided
** with the distribution.
**
** Subject to the terms and conditions of this license, each
** copyright holder and contributor hereby grants to those receiving
** rights under this license a perpetual, worldwide, non-exclusive,
** no-charge, royalty-free, irrevocable (except for failure to
** satisfy the conditions of this license) patent license to make,
** have made, use, offer to sell, sell, import, and otherwise
** transfer this software, where such license applies only to those
** patent claims, already acquired or hereafter acquired, licensable
** by such copyright holder or contributor that are necessarily
** infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright
** holders and non-copyrightable additions of contributors, in
** source or binary form) alone; or
**
** (b) combination of their Contribution(s) with the work of
** authorship to which such Contribution(s) was added by such
** copyright holder or contributor, if, at the time the Contribution
** is added, such addition causes such combination to be necessarily
** infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any
** copyright holder or contributor is granted under this license,
** whether expressly, by implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
** CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
** USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
** AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
** ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/

#include "ubus_prpl.h"

#include <stdio.h>            /* system() */
#include <stdlib.h>           /* system() */
#include <string.h>
#include <unistd.h>           /* access() */

#include <amxc/amxc_macros.h> /* UNUSED */

#include "mod_xpon_macros.h"  /* ARRAY_SIZE() */
#include "mod_xpon_trace.h"
#include "set_of_indexes.h"

#ifndef LINE_MAX
#define LINE_MAX 256
#endif

static const char* s_ubus_cli_cmd = NULL;

static const char* UBUS_PATHS[] = {
    "/bin/ubus", "/usr/bin/ubus", "/usr/local/bin/ubus"
};

static const char* const UBUS_LIST_OUTPUT_FILE = "/tmp/ubus_list.txt";

/**
 * Initialize the ubus_prpl part.
 *
 * The module must call this function once at startup.
 *
 * @return true on success, else false
 */
bool ubus_prpl_init(void) {
    bool rv = false;
    int i;

    const int n_paths = ARRAY_SIZE(UBUS_PATHS);
    for(i = 0; i < n_paths; ++i) {
        if(access(UBUS_PATHS[i], X_OK) == 0) {
            s_ubus_cli_cmd = UBUS_PATHS[i];
            break;
        }
    }
    if(NULL == s_ubus_cli_cmd) {
        SAH_TRACEZ_ERROR(ME, "No ubus cli command found");
        goto exit;
    }
    rv = true;
exit:
    return rv;
}

/**
 * Run 'ubus list' command for a certain prpl xpon_onu object.
 *
 * @param[in] path  path to prpl xpon_onu template object, e.g.
 *                  "xpon_onu.1.software_image"
 *
 * The function runs following command:
 * 'ubus list <prpl_path>*' (it appends star sign to @a prpl_path),
 * and it writes the output to UBUS_LIST_OUTPUT_FILE.
 *
 * @return true on success, else false
 */
static bool ubus_list_objects(const char* const prpl_path) {

    bool rv = false;

    char cmd[128];
    snprintf(cmd, 128, "%s list %s* > %s", s_ubus_cli_cmd, prpl_path,
             UBUS_LIST_OUTPUT_FILE);
    unlink(UBUS_LIST_OUTPUT_FILE);
    SAH_TRACEZ_DEBUG(ME, "%s", cmd);

    if(system(cmd) != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to run 'ubus list' command");
        goto exit;
    }
    if(access(UBUS_LIST_OUTPUT_FILE, F_OK) != 0) {
        SAH_TRACEZ_ERROR(ME, "%s does not exist", UBUS_LIST_OUTPUT_FILE);
        goto exit;
    }
    rv = true;

exit:
    return rv;
}

static bool extract_indexes(const char* const prpl_path,
                            amxc_string_t* const indexes) {

    bool rv = false;
    FILE* file = NULL;
    char prpl_path_with_dot[256];

    set_of_indexes_t set;
    set_of_indexes_init(&set);

    amxc_string_t line_with_prpl_path;
    amxc_string_init(&line_with_prpl_path, 0);

    SAH_TRACEZ_DEBUG(ME, "prpl_path='%s'", prpl_path);

    file = fopen(UBUS_LIST_OUTPUT_FILE, "r");
    when_null_trace(file, exit, ERROR, "Failed to open %s", UBUS_LIST_OUTPUT_FILE);

    char buf[LINE_MAX];
    snprintf(prpl_path_with_dot, 256, "%s.", prpl_path);
    const size_t len = strlen(prpl_path_with_dot);

    const char* number = NULL;
    uint32_t index;
    while(fgets(buf, LINE_MAX, file)) {
        buf[strcspn(buf, "\n")] = 0; /* remove trailing newline */

        if(strncmp(buf, prpl_path_with_dot, len) == 0) {
            amxc_string_set(&line_with_prpl_path, buf);
            amxc_string_replace(&line_with_prpl_path, prpl_path_with_dot, "", /*max=*/ 1);
            if(amxc_string_is_numeric(&line_with_prpl_path)) {
                number = amxc_string_get(&line_with_prpl_path, 0);
                index = (uint32_t) atoi(number);
                set_of_indexes_add_index(&set, index);
            }
            amxc_string_clean(&line_with_prpl_path);
        }
    }
    fclose(file);
    unlink(UBUS_LIST_OUTPUT_FILE);

    if(!set_of_indexes_get_indexes_as_string(&set, indexes)) {
        SAH_TRACEZ_ERROR(ME, "Failed to convert set of indexes to string");
        goto exit;
    }

    rv = true;

exit:
    set_of_indexes_clean(&set);
    amxc_string_clean(&line_with_prpl_path);

    return rv;
}


/**
 * Get the instances of a template object in the prpl xpon_onu DM.
 *
 * @param[in] prpl_path    path to template object in the prpl xpon_onu DM,
 *                         e.g., "xpon_onu.1.software_image"
 * @param[in,out] indexes  function returns list of instance indexes via this
 *                         parameter, formatted as comma-separated integers.
 *                         See below for an example.
 *
 * Example:
 * Assume @a path is "xpon_onu.1.software_image" and that the function finds out
 * that following instances exist:
 * - xpon_onu.1.software_image.1
 * - xpon_onu.1.software_image.2
 * Then it assigns "1,2" to @a indexes.
 *
 * @return true on success, else false
 */
bool ubus_prpl_get_indexes(const char* const prpl_path,
                           amxc_string_t* const indexes) {

    bool rv = false;

    when_null(prpl_path, exit);
    when_null(indexes, exit);

    if(!ubus_list_objects(prpl_path)) {
        goto exit;
    }

    if(!extract_indexes(prpl_path, indexes)) {
        goto exit;
    }
    rv = true;

exit:
    return rv;
}

