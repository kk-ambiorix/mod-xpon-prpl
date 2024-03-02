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

#include "object_utils.h"

#ifdef _DEBUG_
#include <stdio.h>  /* printf() */
#include <unistd.h> /* STDOUT_FILENO */
#endif

#include <amxc/amxc_macros.h> /* when_null() */

#include "dm_info.h"
#include "mod_xpon_trace.h"

static bool process_key(object_id_t id,
                        const amxc_var_t* const params_input,
                        amxc_var_t* ret,
                        const char* const path) {

    bool rv = false;

    const object_info_t* const object_info = dm_get_object_info(id);
    when_null(object_info, exit);

    const char* const key_name = object_info->prpl_key_name; /* alias */
    when_null_trace(key_name, exit, ERROR, "%s: no key", path);

    const amxc_var_t* const key_value =
        amxc_var_get_key(params_input, key_name, AMXC_VAR_FLAG_DEFAULT);
    if(NULL == key_value) {
        SAH_TRACEZ_ERROR(ME, "%s: key '%s' does not occur in 'params'", path, key_name);
        goto exit;
    }

    amxc_var_t* keys = amxc_var_add_key(amxc_htable_t, ret, "keys", NULL);
    when_null_trace(keys, exit, ERROR, "Failed to add 'keys' to 'ret'");
    amxc_var_t* bbf_key_value = amxc_var_add_new_key(keys, object_info->bbf_key_name);

    if(NULL == bbf_key_value) {
        SAH_TRACEZ_ERROR(ME, "%s: failed to add variant for key to 'keys'", path);
        goto exit;
    }
    if(amxc_var_copy(bbf_key_value, key_value)) {
        SAH_TRACEZ_ERROR(ME, "%s: failed to copy key value", path);
        goto exit;
    }

    rv = true;
exit:
    return rv;
}

static bool process_other_params(object_id_t id,
                                 const amxc_var_t* const params_input,
                                 amxc_var_t* ret) {
    bool rv = false;
    when_null(params_input, exit);
    when_null(ret, exit);

    const amxc_htable_t* const htable = amxc_var_constcast(amxc_htable_t, params_input);
    when_null(htable, exit);

    const param_info_t* param_info;
    uint32_t n_params;

    if(!dm_get_object_param_info(id, &param_info, &n_params)) {
        SAH_TRACEZ_ERROR(ME, "Failed to get info about params of object with ID=%d", id);
        goto exit;
    }

    amxc_var_t* params = amxc_var_add_key(amxc_htable_t, ret, "parameters", NULL);
    when_null_trace(params, exit, ERROR, "Failed to add 'parameters' to 'ret'");

    uint32_t i;
    for(i = 0; i < n_params; ++i) {
        if(amxc_htable_contains(htable, param_info[i].prpl_name)) {
            SAH_TRACEZ_DEBUG(ME, "params_input contains '%s'", param_info[i].prpl_name);
            switch(param_info[i].type) {
            case AMXC_VAR_ID_BOOL:
            {
                const bool bval = GET_BOOL(params_input, param_info[i].prpl_name);
                amxc_var_add_key(bool, params, param_info[i].bbf_name, bval);
                break;
            }
            case AMXC_VAR_ID_CSTRING:
            {
                const char* cval = GET_CHAR(params_input, param_info[i].prpl_name);
                amxc_var_add_key(cstring_t, params, param_info[i].bbf_name, cval);
                break;
            }
            case AMXC_VAR_ID_UINT32:
            {
                const uint32_t uval = GET_UINT32(params_input, param_info[i].prpl_name);
                amxc_var_add_key(uint32_t, params, param_info[i].bbf_name, uval);
                break;
            }
            case AMXC_VAR_ID_INT32:
            {
                const int32_t uval = GET_INT32(params_input, param_info[i].prpl_name);
                amxc_var_add_key(int32_t, params, param_info[i].bbf_name, uval);
                break;
            }
            case AMXC_VAR_ID_CSV_STRING:
            {
                const char* cval = GET_CHAR(params_input, param_info[i].prpl_name);
                amxc_var_add_key(csv_string_t, params, param_info[i].bbf_name, cval);
                break;
            }
            default:
                SAH_TRACEZ_ERROR(ME, "Variant type %d: not supported", param_info[i].type);
                break;
            }
        }
    }

    rv = true;

exit:
    return rv;
}

/**
 * Convert param values from ONU HAL agent to format expected by tr181-xpon.
 *
 * @param[in] id           object ID
 * @param[in] params       parameter values of a prpl xpon_onu object
 * @param[in] extract_key  true if the params belong to an instance of a
 *                         template object. Then the function must extract the
 *                         value of the key of the template object from @a
 *                         params and add it to @a ret.
 * @param[in,out] ret      the function returns the result via this parameter.
 *                         See below for more info.
 * @param[in] path         path of object the param values belong to. For
 *                         logging only.
 *
 * In essence the function converts @a params coming from an ONU HAL agent to
 * @a ret to pass the parameter values to the tr181-xpon plugin.
 *
 * The param @a ret is an htable with following keys upon success:
 * - 'parameters': values for the parameters of the object
 * - 'keys': only present in @a ret when querying an instance. It should
 *           contain values for the keys of the template object.
 *
 * As part of the conversion, the function translates the prpl xpon_onu
 * parameter names to BBF TR-181 XPON parameter names, e.g. it translates
 * "SerialNumber"  to "serial_number".
 *
 * @return true on success, else false
 */
bool obj_process_object_params(object_id_t id,
                               const amxc_var_t* const params,
                               bool extract_key,
                               amxc_var_t* ret,
                               const char* const path) {
    bool rv = false;
    when_null_trace(params, exit, ERROR, "params is NULL");

    const object_info_t* const object_info = dm_get_object_info(id);
    when_null(object_info, exit);

    if(amxc_var_type_of(params) != AMXC_VAR_ID_LIST) {
        SAH_TRACEZ_ERROR(ME, "%s: type of 'params' = %d != list", path,
                         amxc_var_type_of(params));
        goto exit;
    }

    const amxc_llist_t* const list = amxc_var_constcast(amxc_llist_t, params);
    when_null_trace(list, exit, ERROR, "list is NULL");
    when_false_trace(amxc_llist_size(list) == 1, exit, ERROR, "size(list) != 1");

    const amxc_llist_it_t* const it_first = amxc_llist_get_first(list);
    when_null_trace(it_first, exit, ERROR, "it_first is NULL");

    const amxc_var_t* const params_table = amxc_var_from_llist_it(it_first);
    when_null_trace(params_table, exit, ERROR, "params_table is NULL");

    const uint32_t ptype = amxc_var_type_of(params_table);
    when_false_trace(ptype == AMXC_VAR_ID_HTABLE, exit, ERROR,
                     "type_of(params_table) = %d != htable", ptype);

    amxc_var_set_type(ret, AMXC_VAR_ID_HTABLE);

    if(extract_key) {
        if(!process_key(id, params_table, ret, path)) {
            goto exit;
        }
    }

    process_other_params(id, params_table, ret);
#ifdef _DEBUG_
    printf("mod_xpon_prpl: obj_process_object_params(): dump(ret):\n");
    amxc_var_dump(ret, STDOUT_FILENO);
#endif

    rv = true;

exit:
    if(false == rv) {
        amxc_var_clean(ret);
    }

    return rv;
}



