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

#include "southbound_if.h"

#include <string.h> /* strlen() */

#ifdef _DEBUG_
#include <stdio.h>  /* printf() */
#include <unistd.h> /* STDOUT_FILENO */
#endif

#include <amxc/amxc_macros.h> /* when_false() */

#include "mod_xpon_trace.h"

static const int AMXB_CALL_TIMEOUT_S = 3; /* seconds */

/**
 * Construct string with dot appended.
 *
 * @param[in] path                input string
 * @param[in,out] path_with_dot   function assigns @a path to this parameter,
 *                                and it appends a dot if it does not end with a
 *                                dot yet.
 */
static void string_append_dot(const char* const path,
                              amxc_string_t* path_with_dot) {

    const size_t len = strlen(path);
    when_false(len != 0, exit);

    amxc_string_set(path_with_dot, path);

    if(path[len - 1] != '.') {
        amxc_string_append(path_with_dot, ".", 1);
    }

exit:
    return;
}

/**
 * Wrapper around amxb_call().
 *
 * @param[in] ctx        bus context
 * @param[in] path       object in prpl xpon_onu DM
 * @param[in] method     name of the function being called
 * @param[in] args       the function arguments in a amxc variant htable type.
 *                       The caller can pass NULL if @a method does not have any
 *                       arguments.
 * @param[in,out] ret    will contain the return value(s). The caller can pass
 *                       NULL if he does not expect any return value(s).
 *
 * @return true on success, else false
 */
static bool call_function_common(amxb_bus_ctx_t* ctx,
                                 const amxc_string_t* const path,
                                 const char* const method,
                                 amxc_var_t* args,
                                 amxc_var_t* ret) {
    bool rv = false;

    amxc_string_t path_dot;/* path with dot appended */
    amxc_string_init(&path_dot, 0);

    when_null_trace(ctx, exit, ERROR, "No bus context");

    const char* const path_cstr = amxc_string_get(path, 0);
    string_append_dot(path_cstr, &path_dot);
    const char* const path_dot_cstr = amxc_string_get(&path_dot, 0);

    const int rc =
        amxb_call(ctx, path_dot_cstr, method, args, ret, AMXB_CALL_TIMEOUT_S);
    if(rc) {
        SAH_TRACEZ_ERROR(ME, "amxb_call %s%s() failed: rc=%d", path_dot_cstr, method, rc);
        goto exit;
    }

    rv = true;
exit:
    amxc_string_clean(&path_dot);
    return rv;
}



/**
 * Call enable() or disable() for a prpl xpon_onu object.
 *
 * @param[in] ctx     bus context
 * @param[in] path    object in prpl xpon_onu DM to call enable() or disable() on
 * @param[in] enable  if true, call enable(), else call disable().
 *
 * Examples for @a prpl_path:
 * - "xpon_onu.2"
 * - "xpon_onu.1.ani.1"
 *
 * @return true on success, else false
 */
bool sbi_enable(amxb_bus_ctx_t* ctx,
                const amxc_string_t* const path,
                bool enable) {

    const char* const method = enable ? "enable" : "disable";

    SAH_TRACEZ_DEBUG(ME, "path='%s' enable=%d", amxc_string_get(path, 0), enable);

    return call_function_common(ctx, path, method, /*args=*/ NULL, /*ret=*/ NULL);
}

#ifdef _DEBUG_
static void dump_param_values(const char* const func_name,
                              const amxc_string_t* const path,
                              const amxc_var_t* const param_values) {

    const char* const path_cstr = amxc_string_get(path, 0);
    printf("mod_xpon_prpl: %s.%s(): dump(param_values):\n", path_cstr, func_name);
    amxc_var_dump(param_values, STDOUT_FILENO);
}
#endif

/**
 * Call get() on a prpl xpon_onu object to get the values of its params.
 *
 * @param[in] ctx               bus context
 * @param[in] path              object in prpl xpon_onu DM to call get() on
 * @param[in,out] param_values  function returns param values via this parameter
 *
 * Example:
 * xpon_onu.1.ani.1.tc.onu_activation.get() returns:
 *   {
 *       onu_id = 1,
 *       onu_state = "O2",
 *       serial_number = "ABCD12345678"
 *       vendor_id = "XYZ1",
 *   }
 *
 * Note - the values are some test values, and do not come from a real setup.
 *
 * @return true on success, else false
 */
bool sbi_query_object(amxb_bus_ctx_t* ctx,
                      const amxc_string_t* const path,
                      amxc_var_t* const param_values) {

    const bool rv = call_function_common(ctx, path, "get", /*args=*/ NULL, param_values);

#ifdef _DEBUG_
    if(rv) {
        dump_param_values("query_object", path, param_values);
    }
#endif
    return rv;
}

/**
 * Call get_params() on a prpl xpon_onu object to get certain param values.
 *
 * @param[in] ctx               bus context
 * @param[in] path              object in prpl xpon_onu DM to call get_params() on
 * @param[in] names             list of param names to be queried, formatted as
 *                              a comma-separated list.
 * @param[in,out] param_values  function returns param values via this parameter
 *
 * Example:
 * xpon_onu.1.ani.1.transceiver.1.get_params("rx_power") returns:
 *   {
 *       rx_power = -18123
 *   }
 *
 * Note - the value is some test value, and does not come from a real setup.
 *
 * @return true on success, else false
 */
bool sbi_query_params(amxb_bus_ctx_t* ctx,
                      const amxc_string_t* const path,
                      const char* const names,
                      amxc_var_t* const param_values) {

    amxc_var_t args;
    amxc_var_init(&args);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "names", names);

    const bool rv = call_function_common(ctx, path, "get_params", &args, param_values);

#ifdef _DEBUG_
    if(rv) {
        dump_param_values("query_params", path, param_values);
    }
#endif

    amxc_var_clean(&args);
    return rv;
}

