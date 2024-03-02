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

#include "pon_ctrl.h"

#include <stdio.h>  /* snprintf() */
#include <string.h> /* strncmp() */

#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>
#include <amxm/amxm.h>        /* amxm_callback_t */

#include <amxp/amxp_signal.h> /* required by amxb.h */
#include <amxp/amxp_slot.h>   /* required by amxb.h */
#include <amxd/amxd_types.h>  /* required by amxb.h */
#include <amxb/amxb.h>        /* amxb_bus_ctx_t */

#include "dm_info.h"
#include "mod_xpon_macros.h" /* ARRAY_SIZE() */
#include "mod_xpon_trace.h"
#include "notif.h"           /* notif_subscribe() */
#include "object_utils.h"    /* obj_process_object_params() */
#include "southbound_if.h"   /* sbi_enable() */
#include "ubus_prpl.h"       /* ubus_prpl_get_indexes() */

#define MOD_PON_CTRL "pon_ctrl"

static amxm_module_t* s_pon_ctrl_module = NULL;
static amxb_bus_ctx_t* s_bus_ctx = NULL;

#ifndef MAX_NR_OF_ONUS
#error MAX_NR_OF_ONUS is not defined
#endif

static uint32_t s_max_nr_of_onus = MAX_NR_OF_ONUS;

/**
 * Set the max nr of ONUs on this board.
 *
 * @param[in] args  the variant must be an uint32_t with the max nr of ONUs
 *
 * @return 0 on success
 * @return -1 on error
 */
static int set_max_nr_of_onus(UNUSED const char* function_name,
                              amxc_var_t* args,
                              UNUSED amxc_var_t* ret) {
    int rc = -1;

    when_null(args, exit);

    const uint32_t max_nr_of_onus = amxc_var_constcast(uint32_t, args);
    if((0 == max_nr_of_onus) || (max_nr_of_onus > MAX_NR_OF_ONUS)) {
        SAH_TRACEZ_ERROR(ME, "max_nr_of_onus=%d is not in [1, %d]",
                         max_nr_of_onus, MAX_NR_OF_ONUS);
        goto exit;
    }

    if(s_max_nr_of_onus != max_nr_of_onus) {
        SAH_TRACEZ_INFO(ME, "max_nr_of_onus: %d -> %d", s_max_nr_of_onus,
                        max_nr_of_onus);
        s_max_nr_of_onus = max_nr_of_onus;
    }

    rc = 0;

exit:
    return rc;
}

/**
 * The read-write Enable field of an object in the XPON DM was changed.
 *
 * @param[in] args  must be an htable with values for the keys 'path' and
 *                  'enable'.
 *                  - 'path': path of object whose Enable field was changed,
 *                     e.g., "XPON.ONU.1", or "XPON.ONU.1.ANI.1"
 *                  - 'enable': true if field was set to true, else false
 *
 * Forward the change to the affected ONU HAL agent.
 *
 * @return 0 on success
 * @return -1 on error
 */
static int set_enable(UNUSED const char* function_name,
                      amxc_var_t* args,
                      UNUSED amxc_var_t* ret) {

    int rc = -1;
    amxc_string_t prpl_path;

    amxc_string_init(&prpl_path, 0);

    when_null(args, exit);
    when_false_trace(amxc_var_type_of(args) == AMXC_VAR_ID_HTABLE, exit, ERROR,
                     "args is not an htable");
    when_null_trace(s_bus_ctx, exit, ERROR, "No bus context");

    const char* const path = GET_CHAR(args, "path");
    when_null_trace(path, exit, ERROR, "Failed to extract path");

    const bool enable = GET_BOOL(args, "enable");
    SAH_TRACEZ_INFO(ME, "path='%s' enable=%d", path, enable);

    if(!dm_convert_bbf_path_to_prpl_path(path, &prpl_path)) {
        SAH_TRACEZ_ERROR(ME, "path='%s': failed to convert to prpl path", path);
        goto exit;
    }

    if(!sbi_enable(s_bus_ctx, &prpl_path, enable)) {
        SAH_TRACEZ_ERROR(ME, "path='%s' enable=%d failed", path, enable);
        goto exit;
    }

    rc = 0;

exit:
    amxc_string_clean(&prpl_path);
    return rc;
}


static bool get_indexes(const char* const bbf_path,
                        amxc_string_t* const indexes) {
    bool rv = false;

    amxc_string_t prpl_path;
    amxc_string_init(&prpl_path, 0);

    if(!dm_convert_bbf_path_to_prpl_path(bbf_path, &prpl_path)) {
        SAH_TRACEZ_ERROR(ME, "path='%s': failed to convert to prpl path", bbf_path);
        goto exit;
    }
    const char* const prpl_path_cstr = amxc_string_get(&prpl_path, 0);

    if(!ubus_prpl_get_indexes(prpl_path_cstr, indexes)) {
        SAH_TRACEZ_ERROR(ME, "path='%s': failed to get instances", bbf_path);
        goto exit;
    }

    const char* const idxs = amxc_string_get(indexes, 0);
    SAH_TRACEZ_DEBUG(ME, "indexes='%s'", idxs ? idxs : "");

    rv = true;

exit:
    amxc_string_clean(&prpl_path);

    return rv;
}

/**
 * Check xpon_onu instances.
 *
 * @param[in,out] indexes  function returns list of xpon_onu instances via
 *                         this parameter, formatted as comma-separated
 *                         integers. See below for an example.
 *
 * If this function finds out that an xpon_onu instance exists, it calls
 * notif_subscribe() to subscribe on the notifications from that xpon_onu
 * instance (if it did not subscribe yet).
 *
 * Note: this module assumes that an ONU HAL agent keeps running until reboot.
 * Hence it assumes an xpon_onu instance does not disappear. And hence it does
 * not unsubscribe from any xpon_onu instances.
 *
 * Example:
 * if the function finds out that xpon_onu.1 and xpon_onu.2 exist, it assigns
 * "1,2" to @a indexes.
 */
static void get_onu_indexes(amxc_string_t* const indexes) {

    bool first = true;
    uint32_t i;
    char onu_path[16];
    amxb_bus_ctx_t* bus_ctx;
    uint32_t index;

    for(i = 0; i < s_max_nr_of_onus; ++i) {
        index = (i + 1);
        snprintf(onu_path, 16, "xpon_onu.%d", index);
        bus_ctx = amxb_be_who_has(onu_path);

        if(bus_ctx) {
            if(first) {
                amxc_string_appendf(indexes, "%d", index);
                first = false;
            } else {
                amxc_string_appendf(indexes, ",%d", index);
            }

            if(NULL == s_bus_ctx) {
                SAH_TRACEZ_DEBUG(ME, "Set bus context");
                s_bus_ctx = bus_ctx;
            } else if(bus_ctx != s_bus_ctx) {
                SAH_TRACEZ_ERROR(ME, "bus ctx [%p] != s_bus_ctx [%p]",
                                 bus_ctx, s_bus_ctx);
            }

            if(!notif_is_subscribed(index)) {
                notif_subscribe(bus_ctx, index);
            }
        } else {
            SAH_TRACEZ_DEBUG(ME, "%s does not exist", onu_path);
        }
    }
}

/**
 * Query which instances exist for a template object.
 *
 * @param[in] args     must be htable with the key 'path'. The path must refer
 *                     to a template object, e.g., "XPON.ONU.1.SoftwareImage".
 * @param[in,out] ret  the function returns the result via this parameter. See
 *                     below for more info.
 *
 * The param @a ret is an htable with following key upon success:
 * - 'indexes': a string with comma-separated integers representing the instance
 *              indexes. It's an empty string if no instances exist.
 *
 * @return 0 on success
 * @return -1 on error
 */
int get_list_of_instances(UNUSED const char* function_name,
                          amxc_var_t* args,
                          amxc_var_t* ret) {
    int rc = -1;

    when_null(args, exit);
    when_null(ret, exit);

    const char* const path = amxc_var_constcast(cstring_t, args);
    if(!path) {
        SAH_TRACEZ_ERROR(ME, "Failed to extract path");
        goto exit;
    }
    SAH_TRACEZ_INFO(ME, "path='%s'", path);

    const object_id_t id = dm_get_object_id(path);
    if(obj_id_unknown == id) {
        SAH_TRACEZ_ERROR(ME, "Failed to get ID for '%s'", path);
        goto exit;
    }

    amxc_var_set_type(ret, AMXC_VAR_ID_HTABLE);

    amxc_string_t indexes;
    amxc_string_init(&indexes, 0);

    if(obj_id_onu == id) {
        get_onu_indexes(&indexes);
    } else {
        if(!get_indexes(path, &indexes)) {
            goto exit_cleanup;
        }
    }

    const char* const idxs = amxc_string_get(&indexes, 0);
    if(amxc_var_add_key(cstring_t, ret, "indexes", idxs ? idxs : "") == NULL) {
        SAH_TRACEZ_ERROR(ME, "Failed to add indexes");
        goto exit_cleanup;
    }

    rc = 0;

exit_cleanup:
    amxc_string_clean(&indexes);

exit:
    return rc;
}

static bool query_object(const amxc_string_t* const bbf_path, amxc_var_t* params) {

    bool rv = false;
    const char* const bbf_path_cstr = amxc_string_get(bbf_path, 0);
    SAH_TRACEZ_DEBUG(ME, "bbf_path='%s'", bbf_path_cstr);

    amxc_string_t prpl_path;
    amxc_string_init(&prpl_path, 0);

    if(!dm_convert_bbf_path_to_prpl_path(bbf_path_cstr, &prpl_path)) {
        SAH_TRACEZ_ERROR(ME, "path='%s': failed to convert to prpl path", bbf_path_cstr);
        goto exit;
    }

    if(!sbi_query_object(s_bus_ctx, &prpl_path, params)) {
        goto exit;
    }

    rv = true;
exit:
    amxc_string_clean(&prpl_path);
    return rv;
}

/**
 * Get the parameter values of an object.
 *
 * @param[in] args     must be htable with the key 'path'. It must also have an
 *                     entry with key 'index' when querying an instance of a
 *                     template object.
 * @param[in,out] ret  the function returns the result via this parameter. See
 *                     below for more info.
 *
 * The param @a ret is an htable with following keys upon success:
 * - 'parameters': values for the parameters of the object
 * - 'keys': only present in @a ret when querying an instance. It should
 *           contain values for the keys of the template object.
 *
 * @return 0 on success
 * @return -1 on error
 */
static int get_object_content(UNUSED const char* function_name,
                              amxc_var_t* args,
                              amxc_var_t* ret) {
    int rc = -1;
    amxc_var_t params;
    amxc_string_t bbf_path;

    amxc_var_init(&params); /* will contain the param value(s) */
    amxc_string_init(&bbf_path, 0);

    when_null(args, exit);
    when_null(ret, exit);
    when_false_trace(amxc_var_type_of(args) == AMXC_VAR_ID_HTABLE, exit, ERROR,
                     "args is not an htable");
    when_null_trace(s_bus_ctx, exit, ERROR, "No bus context");

    const char* const path = GET_CHAR(args, "path");
    when_null_trace(path, exit, ERROR, "Failed to extract path");

    const uint32_t index = GET_UINT32(args, "index");
    SAH_TRACEZ_INFO(ME, "path='%s' index=%d", path, index);

    const object_id_t id = dm_get_object_id(path);
    if(obj_id_unknown == id) {
        SAH_TRACEZ_ERROR(ME, "path='%s': failed to get ID", path);
        goto exit;
    }

    amxc_string_set(&bbf_path, path);
    if(index) {
        amxc_string_appendf(&bbf_path, ".%d", index);
    }

    if(!query_object(&bbf_path, &params)) {
        goto exit;
    }

    const bool extract_key = (index != 0);
    if(!obj_process_object_params(id, &params, extract_key, ret,
                                  amxc_string_get(&bbf_path, 0))) {
        goto exit;
    }

    rc = 0;

exit:
    amxc_string_clean(&bbf_path);
    amxc_var_clean(&params);
    return rc;
}

static bool query_params(object_id_t id,
                         const amxc_string_t* const bbf_path,
                         const char* const bbf_param_names,
                         amxc_var_t* params) {

    bool rv = false;
    const char* const bbf_path_cstr = amxc_string_get(bbf_path, 0);
    SAH_TRACEZ_DEBUG(ME, "bbf_path='%s'", bbf_path_cstr);

    amxc_string_t prpl_path;
    amxc_string_t prpl_param_names;

    amxc_string_init(&prpl_path, 0);
    amxc_string_init(&prpl_param_names, 0);

    if(!dm_convert_bbf_path_to_prpl_path(bbf_path_cstr, &prpl_path)) {
        SAH_TRACEZ_ERROR(ME, "path='%s': failed to convert to prpl path", bbf_path_cstr);
        goto exit;
    }

    if(!dm_convert_param_names(id, bbf_param_names, &prpl_param_names)) {
        goto exit;
    }

    const char* const prpl_param_names_ctr = amxc_string_get(&prpl_param_names, 0);
    if(!sbi_query_params(s_bus_ctx, &prpl_path, prpl_param_names_ctr, params)) {
        goto exit;
    }

    rv = true;
exit:
    amxc_string_clean(&prpl_path);
    amxc_string_clean(&prpl_param_names);
    return rv;
}


/**
 * Get the values of one or more parameters of an object.
 *
 * @param[in] args     must be htable with the keys 'path' and 'names'. 'path'
 *                     must refer to a singleton or an instance. 'names' is a
 *                     list of param names to be queried, formatted as a
 *                     comma-separated list.
 * @param[in,out] ret  the function returns the result via this parameter. See
 *                     below for more info.
 *
 * The param @a ret is an htable with following keys upon success:
 * - 'parameters': values for the requested parameter(s) of the object
 *
 * @return 0 on success
 * @return -1 on error
 */
static int get_param_values(UNUSED const char* function_name,
                            amxc_var_t* args,
                            amxc_var_t* ret) {
    int rc = -1;
    amxc_var_t params;
    amxc_string_t bbf_path;

    amxc_var_init(&params); /* will contain the param value(s) */
    amxc_string_init(&bbf_path, 0);

    when_null(args, exit);
    when_null(ret, exit);
    when_false_trace(amxc_var_type_of(args) == AMXC_VAR_ID_HTABLE, exit, ERROR,
                     "args is not an htable");
    when_null_trace(s_bus_ctx, exit, ERROR, "No bus context");

    const char* const path = GET_CHAR(args, "path");
    when_null_trace(path, exit, ERROR, "Failed to extract path");

    const char* const names = GET_CHAR(args, "names"); /* BBF param names */
    when_null_trace(names, exit, ERROR, "Failed to extract 'names'");

    SAH_TRACEZ_DEBUG(ME, "path='%s' names=%s", path, names);

    const object_id_t id = dm_get_object_id(path);
    if(obj_id_unknown == id) {
        SAH_TRACEZ_ERROR(ME, "path='%s': failed to get ID", path);
        goto exit;
    }

    amxc_string_set(&bbf_path, path);

    if(!query_params(id, &bbf_path, names, &params)) {
        goto exit;
    }

    if(!obj_process_object_params(id, &params, /*extract_key=*/ false, ret,
                                  amxc_string_get(&bbf_path, 0))) {
        goto exit;
    }

    rc = 0;

exit:
    amxc_string_clean(&bbf_path);
    amxc_var_clean(&params);
    return rc;
}


typedef struct _func_info {
    const char* const name;
    amxm_callback_t cb;
} func_info_t;

static const func_info_t MOD_PON_CTRL_FUNCS[] = {
    { .name = "set_max_nr_of_onus", .cb = set_max_nr_of_onus },
    { .name = "set_enable", .cb = set_enable },
    { .name = "get_list_of_instances", .cb = get_list_of_instances },
    { .name = "get_object_content", .cb = get_object_content },
    { .name = "get_param_values", .cb = get_param_values }
};


/**
 * Register the 'pon_ctrl' namespace.
 *
 * The module must call this function once at startup.
 *
 * @return true on success, else false
 */
bool pon_ctrl_init(void) {

    bool rv = false;
    int rc;
    int i;

    amxm_shared_object_t* so = amxm_so_get_current();
    rc = amxm_module_register(&s_pon_ctrl_module, so, MOD_PON_CTRL);

    when_failed_trace(rc, exit, ERROR, "Failed to register %s namespace (rc=%d)", MOD_PON_CTRL, rc);
    when_null_trace(s_pon_ctrl_module, exit, ERROR, "Failed to register %s namespace", MOD_PON_CTRL);

    const int n_functions = ARRAY_SIZE(MOD_PON_CTRL_FUNCS);
    for(i = 0; i < n_functions; ++i) {
        amxm_module_add_function(s_pon_ctrl_module, MOD_PON_CTRL_FUNCS[i].name, MOD_PON_CTRL_FUNCS[i].cb);
    }

    rv = true;

exit:
    return rv;
}

/**
 * Unregister the 'pon_ctrl' namespace.
 *
 * The module must call this function once when stopping.
 */
void pon_ctrl_cleanup(void) {
    amxm_module_deregister(&s_pon_ctrl_module);
}

