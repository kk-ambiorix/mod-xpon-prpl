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

#include "dm_info.h"

#include <string.h> /* strncmp() */

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>

#include "mod_xpon_macros.h" /* ARRAY_SIZE() */
#include "mod_xpon_trace.h"

static const param_info_t ONU_PARAMS[] = {
    { .bbf_name = "Enable", .prpl_name = "enable", .type = AMXC_VAR_ID_BOOL    },
    { .bbf_name = "Version", .prpl_name = "version", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "EquipmentID", .prpl_name = "equipment_id", .type = AMXC_VAR_ID_CSTRING }
};

static const param_info_t SOFTWARE_IMAGE_PARAMS[] = {
    { .bbf_name = "Version", .prpl_name = "version", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "IsCommitted", .prpl_name = "is_committed", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "IsActive", .prpl_name = "is_active", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "IsValid", .prpl_name = "is_valid", .type = AMXC_VAR_ID_BOOL }
};

static const param_info_t ETHERNET_UNI_PARAMS[] = {
    { .bbf_name = "Enable", .prpl_name = "enable", .type = AMXC_VAR_ID_BOOL       },
    { .bbf_name = "Status", .prpl_name = "status", .type = AMXC_VAR_ID_CSTRING    },
    { .bbf_name = "LastChange", .prpl_name = "last_change", .type = AMXC_VAR_ID_UINT32     },
    { .bbf_name = "ANIs", .prpl_name = "ani_list", .type = AMXC_VAR_ID_CSV_STRING },
    { .bbf_name = "InterdomainID", .prpl_name = "interdomain_id", .type = AMXC_VAR_ID_CSTRING    },
    { .bbf_name = "InterdomainName", .prpl_name = "interdomain_name", .type = AMXC_VAR_ID_CSTRING    },
};

static const param_info_t ANI_PARAMS[] = {
    { .bbf_name = "Enable", .prpl_name = "enable", .type = AMXC_VAR_ID_BOOL    },
    { .bbf_name = "Status", .prpl_name = "status", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "LastChange", .prpl_name = "last_change", .type = AMXC_VAR_ID_UINT32  },
    { .bbf_name = "PONMode", .prpl_name = "pon_mode", .type = AMXC_VAR_ID_CSTRING }
};

static const param_info_t GEM_PORT_PARAMS[] = {
    { .bbf_name = "Direction", .prpl_name = "direction", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "PortType", .prpl_name = "port_type", .type = AMXC_VAR_ID_CSTRING }
};

static const param_info_t TRANSCEIVER_PARAMS[] = {
    { .bbf_name = "Identifier", .prpl_name = "identifier", .type = AMXC_VAR_ID_UINT32  },
    { .bbf_name = "VendorName", .prpl_name = "vendor_name", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "VendorPartNumber", .prpl_name = "vendor_part_number", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "VendorRevision", .prpl_name = "vendor_revision", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "PONMode", .prpl_name = "pon_mode", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "Connector", .prpl_name = "connector", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "NominalBitRateDownstream", .prpl_name = "nominal_bit_rate_downstream", .type = AMXC_VAR_ID_UINT32  },
    { .bbf_name = "NominalBitRateUpstream", .prpl_name = "nominal_bit_rate_upstream", .type = AMXC_VAR_ID_UINT32  },
    { .bbf_name = "RxPower", .prpl_name = "rx_power", .type = AMXC_VAR_ID_INT32   },
    { .bbf_name = "TxPower", .prpl_name = "tx_power", .type = AMXC_VAR_ID_INT32   },
    { .bbf_name = "Voltage", .prpl_name = "voltage", .type = AMXC_VAR_ID_UINT32  },
    { .bbf_name = "Bias", .prpl_name = "bias", .type = AMXC_VAR_ID_UINT32  },
    { .bbf_name = "Temperature", .prpl_name = "temperature", .type = AMXC_VAR_ID_INT32   }
};

static const param_info_t ONU_ACTIVATION_PARAMS[] = {
    { .bbf_name = "ONUState", .prpl_name = "onu_state", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "VendorID", .prpl_name = "vendor_id", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "SerialNumber", .prpl_name = "serial_number", .type = AMXC_VAR_ID_CSTRING },
    { .bbf_name = "ONUID", .prpl_name = "onu_id", .type = AMXC_VAR_ID_UINT32  }
};

static const param_info_t PERFORMANCE_THRESHOLDS_PARAMS[] = {
    { .bbf_name = "SignalFail", .prpl_name = "signal_fail", .type = AMXC_VAR_ID_UINT32 },
    { .bbf_name = "SignalDegrade", .prpl_name = "signal_degrade", .type = AMXC_VAR_ID_UINT32 }
};

static const param_info_t TC_ALARMS_PARAMS[] = {
    { .bbf_name = "LOS", .prpl_name = "los", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "LOF", .prpl_name = "lof", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "SF", .prpl_name = "sf", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "SD", .prpl_name = "sd", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "LCDG", .prpl_name = "lcdg", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "TF", .prpl_name = "tf", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "SUF", .prpl_name = "suf", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "MEM", .prpl_name = "mem", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "DACT", .prpl_name = "dact", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "DIS", .prpl_name = "dis", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "MIS", .prpl_name = "mis", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "PEE", .prpl_name = "pee", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "RDI", .prpl_name = "rdi", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "LODS", .prpl_name = "lods", .type = AMXC_VAR_ID_BOOL },
    { .bbf_name = "ROGUE", .prpl_name = "rogue", .type = AMXC_VAR_ID_BOOL }
};

/**
 * Array with info about objects in the XPON DM ad the prpl xpon_onu DM.
 *
 * The 'id' of the element at index 'i' must have 'i' as value.
 */
static const object_info_t OBJECT_INFO[obj_id_nbr] = {
    {
        .id = obj_id_onu,
        .bbf_path = "XPON.ONU",
        .prpl_path = "xpon_onu",
        .bbf_key_name = NAME_VALUE,
        .prpl_key_name = "name",
        .params = ONU_PARAMS,
        .n_params = ARRAY_SIZE(ONU_PARAMS)
    },
    {
        .id = obj_id_software_image,
        .bbf_path = "XPON.ONU.x.SoftwareImage",
        .prpl_path = "xpon_onu.x.software_image",
        .bbf_key_name = "ID",
        .prpl_key_name = "id",
        .key_max_value = 1,
        .params = SOFTWARE_IMAGE_PARAMS,
        .n_params = ARRAY_SIZE(SOFTWARE_IMAGE_PARAMS)
    },
    {
        .id = obj_id_ethernet_uni,
        .bbf_path = "XPON.ONU.x.EthernetUNI",
        .prpl_path = "xpon_onu.x.ethernet_uni",
        .bbf_key_name = NAME_VALUE,
        .prpl_key_name = "name",
        .params = ETHERNET_UNI_PARAMS,
        .n_params = ARRAY_SIZE(ETHERNET_UNI_PARAMS)
    },
    {
        .id = obj_id_ani,
        .bbf_path = "XPON.ONU.x.ANI",
        .prpl_path = "xpon_onu.x.ani",
        .bbf_key_name = NAME_VALUE,
        .prpl_key_name = "name",
        .params = ANI_PARAMS,
        .n_params = ARRAY_SIZE(ANI_PARAMS)
    },
    {
        .id = obj_id_gem_port,
        .bbf_path = "XPON.ONU.x.ANI.x.TC.GEM.Port",
        .prpl_path = "xpon_onu.x.ani.x.tc.gem.port",
        .bbf_key_name = "PortID",
        .prpl_key_name = "port_id",
        .key_max_value = 65534,
        .params = GEM_PORT_PARAMS,
        .n_params = ARRAY_SIZE(GEM_PORT_PARAMS)
    },
    {
        .id = obj_id_transceiver,
        .bbf_path = "XPON.ONU.x.ANI.x.Transceiver",
        .prpl_path = "xpon_onu.x.ani.x.transceiver",
        .bbf_key_name = "ID",
        .prpl_key_name = "id",
        .key_max_value = 1,
        .params = TRANSCEIVER_PARAMS,
        .n_params = ARRAY_SIZE(TRANSCEIVER_PARAMS)
    },
    {
        .id = obj_id_ani_tc_onu_activation,
        .bbf_path = "XPON.ONU.x.ANI.x.TC.ONUActivation",
        .prpl_path = "xpon_onu.x.ani.x.tc.onu_activation",
        .bbf_key_name = NULL,
        .prpl_key_name = NULL,
        .params = ONU_ACTIVATION_PARAMS,
        .n_params = ARRAY_SIZE(ONU_ACTIVATION_PARAMS)
    },
    {
        .id = obj_id_ani_tc_performance_thresholds,
        .bbf_path = "XPON.ONU.x.ANI.x.TC.PerformanceThresholds",
        .prpl_path = "xpon_onu.x.ani.x.tc.performance_thresholds",
        .bbf_key_name = NULL,
        .prpl_key_name = NULL,
        .params = PERFORMANCE_THRESHOLDS_PARAMS,
        .n_params = ARRAY_SIZE(PERFORMANCE_THRESHOLDS_PARAMS)
    },
    {
        .id = obj_id_ani_tc_alarms,
        .bbf_path = "XPON.ONU.x.ANI.x.TC.Alarms",
        .prpl_path = "xpon_onu.x.ani.x.tc.alarms",
        .bbf_key_name = NULL,
        .prpl_key_name = NULL,
        .params = TC_ALARMS_PARAMS,
        .n_params = ARRAY_SIZE(TC_ALARMS_PARAMS)
    }
};

struct bbf_vs_prpl_entry {
    const char* bbf_name;
    const char* prpl_name;
};

static const struct bbf_vs_prpl_entry BBF_vs_PRPL_ENTRIES[] = {
    { .bbf_name = "SoftwareImage", .prpl_name = "software_image" },
    { .bbf_name = "EthernetUNI", .prpl_name = "ethernet_uni" },
    { .bbf_name = "ANI", .prpl_name = "ani" },
    { .bbf_name = "TC", .prpl_name = "tc" },
    { .bbf_name = "GEM", .prpl_name = "gem" },
    { .bbf_name = "Port", .prpl_name = "port" },
    { .bbf_name = "Transceiver", .prpl_name = "transceiver" },
    { .bbf_name = "ONUActivation", .prpl_name = "onu_activation" },
    { .bbf_name = "PerformanceThresholds", .prpl_name = "performance_thresholds" },
    { .bbf_name = "Alarms", .prpl_name = "alarms" }
};

/**
 * Initialize the dm_info part.
 *
 * The function only runs a sanity check on the OBJECT_INFO array.
 *
 * The module must call this function once at startup.
 *
 * @return true on success, else false
 */
bool dm_info_init(void) {
    unsigned int i;
    for(i = 0; i < obj_id_nbr; ++i) {
        if(OBJECT_INFO[i].id != i) {
            SAH_TRACEZ_ERROR(ME, "OBJECT_INFO[%u].id=%d != %u",
                             i, OBJECT_INFO[i].id, i);
            return false;
        }
    }
    return true;
}

/**
 * Convert an actual path to a generic path.
 *
 * @param[in] path  actual path with real instance indexes, e.g.,
 *                  "XPON.ONU.1.ANI.1.Transceiver.2
 * @param[in,out] generic_path  the function returns the generic version of
 *     @a path. The generic version of a path is the path with the last instance
 *     index removed (if there is one), and all remaining instance indexes
 *     replaced by 'x'.
 *
 * The function works for both:
 * - BBF paths such as "XPON.ONU.1.ANI.1.Transceiver.2", and
 * - prpl paths such as "xpon_onu.1.ani.1.transceiver.2".
 *
 * Example:
 * @a path = "XPON.ONU.1.ANI.1.Transceiver.2" =>
 * @a generic_path = "XPON.ONU.x.ANI.x.Transceiver"
 *
 * @return true on success, else false
 */
static bool dm_convert_to_generic_path(const char* const path,
                                       amxc_string_t* const generic_path) {

    bool rv = false;
    when_null(path, exit_no_cleanup);
    when_null(generic_path, exit_no_cleanup);

    amxc_llist_t list;
    amxc_llist_init(&list);

    amxc_string_t input;
    amxc_string_init(&input, 0);
    amxc_string_set(&input, path);

    if(AMXC_STRING_SPLIT_OK != amxc_string_split_to_llist(&input, &list, '.')) {
        SAH_TRACEZ_ERROR(ME, "Failed to split '%s'", path);
        goto exit;
    }

    /* Remove the last elem from 'list' if it's numeric */
    amxc_llist_it_t* last_it = amxc_llist_get_last(&list);
    when_null(last_it, exit);
    amxc_string_t* const last_elem = amxc_string_from_llist_it(last_it);
    if(amxc_string_is_numeric(last_elem)) {
        amxc_llist_take_last(&list);
        amxc_string_list_it_free(last_it);
    }

    amxc_llist_iterate(it, &list) {
        amxc_string_t* part = amxc_string_from_llist_it(it);
        if(amxc_string_is_numeric(part)) {
            amxc_string_set(part, "x");
        }
    }

    if(amxc_string_join_llist(generic_path, &list, '.') != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to generate generic path from '%s'", path);
        goto exit;
    }
    rv = true;

exit:
    amxc_llist_clean(&list, amxc_string_list_it_free);
    amxc_string_clean(&input);

exit_no_cleanup:
    return rv;
}


/**
 * Return the ID of an object.
 *
 * @param[in] path  object path, e.g., "XPON.ONU.1.SoftwareImage", or
 *                  "xpon_onu.1.software_image"
 *
 * Example:
 * if @a path is "XPON.ONU.1.SoftwareImage" or "xpon_onu.1.software_image", the
 * function returns obj_id_software_image.
 *
 * @return one of the value of object_id_t smaller than obj_id_nbr upon
 *         success, else obj_id_unknown
 */
object_id_t dm_get_object_id(const char* const path) {

    object_id_t id = obj_id_unknown;

    when_null(path, exit);

    const bool is_bbf_path = (strncmp(path, "XPON.ONU", 8) == 0) ? true : false;

    amxc_string_t generic_path_str;
    amxc_string_init(&generic_path_str, 0);

    if(!dm_convert_to_generic_path(path, &generic_path_str)) {
        goto exit_cleanup;
    }

    const char* const generic_path = amxc_string_get(&generic_path_str, 0);
    const size_t generic_path_len = strlen(generic_path);

    uint32_t i;
    size_t len;
    const char* other_path;
    for(i = 0; i < obj_id_nbr; ++i) {
        other_path =
            is_bbf_path ? OBJECT_INFO[i].bbf_path : OBJECT_INFO[i].prpl_path;
        len = strlen(other_path);
        if((generic_path_len == len) &&
           (strncmp(generic_path, other_path, len) == 0)) {
            id = OBJECT_INFO[i].id;
            break;
        }
    }

exit_cleanup:
    amxc_string_clean(&generic_path_str);
exit:
    return id;
}

const object_info_t* dm_get_object_info(object_id_t id) {
    if(id < obj_id_nbr) {
        return &OBJECT_INFO[id];
    }

    SAH_TRACEZ_ERROR(ME, "Invalid id [%d]", id);
    return NULL;
}

/**
 * Return info about the params of an object.
 *
 * @param[in] id              object ID
 * @param[in,out] param_info  function passes info about the params of the
 *                            object via this param.
 * @param[in,out] size        nr of elems in @a param_info
 *
 * @return true on success, else false
 */
bool dm_get_object_param_info(object_id_t id, const param_info_t** param_info,
                              uint32_t* size) {
    if(id < obj_id_nbr) {
        *param_info = OBJECT_INFO[id].params;
        *size = OBJECT_INFO[id].n_params;
        return true;
    }
    SAH_TRACEZ_ERROR(ME, "Invalid id [%d]", id);
    return false;
}

static bool convert_path_between_bbf_and_prpl(const char* const input,
                                              amxc_string_t* const output,
                                              bool bbf_to_prpl) {

    bool rv = false;

    when_null(input, exit_no_cleanup);
    when_null(output, exit_no_cleanup);

    const char* const START = bbf_to_prpl ? "XPON.ONU" : "xpon_onu";

    /* Both strings in previous statement have length of 8 chars */
    if(strncmp(input, START, 8) != 0) {
        SAH_TRACEZ_ERROR(ME, "input='%s' does not start with %s", input, START);
        goto exit_no_cleanup;
    }

    amxc_string_t input_str;
    amxc_llist_t list;

    amxc_llist_init(&list);

    amxc_string_init(&input_str, 0);
    amxc_string_set(&input_str, input);
    if(bbf_to_prpl) {
        /* Strip "XPON." from start of string */
        if(amxc_string_remove_at(&input_str, 0, 5) == -1) {
            SAH_TRACEZ_ERROR(ME, "Failed to remove 1st 5 chars from '%s'", input);
            goto exit;
        }
    }
    const char* const input_str_cstr = amxc_string_get(&input_str, 0);

    if(AMXC_STRING_SPLIT_OK != amxc_string_split_to_llist(&input_str, &list, '.')) {
        SAH_TRACEZ_ERROR(ME, "Failed to split '%s'", input_str_cstr);
        goto exit;
    }

    if(amxc_llist_is_empty(&list)) {
        SAH_TRACEZ_ERROR(ME, "list from splitting '%s' is empty", input_str_cstr);
        goto exit;
    }

    /* Translate the 1st element in the list. It is "xpon_onu" or "ONU". */
    const amxc_llist_it_t* const first_it = amxc_llist_get_first(&list);
    when_null_trace(first_it, exit, ERROR, "first_it is NULL");
    amxc_string_t* const first_elem = amxc_string_from_llist_it(first_it);
    amxc_string_set(first_elem, bbf_to_prpl ? "xpon_onu" : "XPON.ONU");

    /* Translate the other elems in the list */
    const char* part_value;
    size_t i;
    const size_t n_entries = ARRAY_SIZE(BBF_vs_PRPL_ENTRIES);
    size_t len;
    const char* old;
    bool found;
    bool first = true;
    amxc_llist_iterate(it, &list) {
        /* Skip the 1st elem. The function already translated that elem. */
        if(first) {
            first = false;
            continue;
        }
        amxc_string_t* part = amxc_string_from_llist_it(it);
        part_value = amxc_string_get(part, 0);
        len = strlen(part_value);
        /**
         * Skip elems which are empty, numeric or equal to 'x'. If the input string
         * ends with a dot, the last element in 'list' is an empty string.
         */
        if(amxc_string_is_empty(part) || amxc_string_is_numeric(part) ||
           ((strlen(part_value) == 1) && (part_value[0] == 'x'))) {
            continue;
        }
        found = false;
        for(i = 0; i < n_entries; ++i) {
            old = bbf_to_prpl ? BBF_vs_PRPL_ENTRIES[i].bbf_name :
                BBF_vs_PRPL_ENTRIES[i].prpl_name;
            if((len == strlen(old)) &&
               (strncmp(part_value, old, len) == 0)) {
                amxc_string_set(part, bbf_to_prpl ? BBF_vs_PRPL_ENTRIES[i].prpl_name :
                                BBF_vs_PRPL_ENTRIES[i].bbf_name);
                found = true;
                break; // out of 'for (i =' loop
            }
        }
        if(!found) {
            SAH_TRACEZ_ERROR(ME, "Failed to translate '%s'", part_value);
        }
    }

    if(amxc_string_join_llist(output, &list, '.') != 0) {
        SAH_TRACEZ_ERROR(ME, "%s: failed to convert it to %s path", input,
                         bbf_to_prpl ? "prpl" : "bbf");
        goto exit;
    }
    SAH_TRACEZ_DEBUG(ME, "%s -> %s", input, amxc_string_get(output, 0));
    rv = true;

exit:
    amxc_llist_clean(&list, amxc_string_list_it_free);
    amxc_string_clean(&input_str);

exit_no_cleanup:
    return rv;
}


/**
 * Convert BBF path to equivalent prpl path.
 *
 * @param[in] bbf_path       path to BBF XPON.ONU object. The string must start
 *                           with "XPON.ONU".
 * @param[in,out] prpl_path  @a bbf_path converted to equivalent prpl path
 *
 * Example:
 * @a bbf_path = "XPON.ONU.1.ANI.1.Transceiver.1" =>
 * @a prpl_path = "xpon_onu.1.ani.1.transceiver.1"
 *
 * @return true on success, else false
 */
bool dm_convert_bbf_path_to_prpl_path(const char* const bbf_path,
                                      amxc_string_t* const prpl_path) {

    return convert_path_between_bbf_and_prpl(bbf_path, prpl_path, true);
}

/**
 * Convert prpl path to equivalent BBF path.
 *
 * @param[in] prpl_path     path to prpl xpon_onu object. The string must start
 *                          with "xpon_onu".
 * @param[in,out] bbf_path  @a prpl_path converted to equivalent BBF path
 *
 * Example:
 * @a prpl_path = "xpon_onu.1.ani.1.transceiver.1" =>
 * @a bbf_path = "XPON.ONU.1.ANI.1.Transceiver.1"
 *
 * @return true on success, else false
 */
bool dm_convert_prpl_path_to_bbf_path(const char* const prpl_path,
                                      amxc_string_t* const bbf_path) {

    return convert_path_between_bbf_and_prpl(prpl_path, bbf_path, false);
}

/**
 * Convert BBF param name(s) to equivalent prpl param name(s).
 *
 * @attention For now the function only supports converting one parameter
 *            of an XPON.ONU.{i}.ANI.1.Transceiver.{i} instance.
 *
 * @param[in] id                    object ID. The function for now only
 *                                  supports obj_id_transceiver.
 * @param[in] bbf_param_names       list of param names to be queried. For now
 *                                  the function supports max 1 parameter, e.g.
 *                                  "RxPower".
 * @param[in,out] prpl_param_names  @a bbf_param_names converted to their
 *                                  equivalent prpl names
 *
 * Example:
 * - @a id == obj_id_transceiver, @a bbf_param_names = "RxPower" =>
 *   @a prpl_param_names = "rx_power"
 *
 * @return true on success, else false
 */
bool dm_convert_param_names(object_id_t id,
                            const char* const bbf_param_names,
                            amxc_string_t* prpl_param_names) {
    bool rv = false;
    if(id != obj_id_transceiver) {
        SAH_TRACEZ_ERROR(ME, "Function only supports transceiver");
        return false;
    }

    if(strchr(bbf_param_names, ',')) {
        SAH_TRACEZ_ERROR(ME, "Function does not support multiple param names");
        return false;
    }

    const param_info_t* param_info;
    uint32_t n_params;

    if(!dm_get_object_param_info(id, &param_info, &n_params)) {
        SAH_TRACEZ_ERROR(ME, "Failed to get info about params of object with ID=%d", id);
        return false;
    }
    uint32_t i;
    for(i = 0; i < n_params; ++i) {
        if(strncmp(bbf_param_names, param_info[i].bbf_name,
                   strlen(param_info[i].bbf_name)) == 0) {
            amxc_string_set(prpl_param_names, param_info[i].prpl_name);
            rv = true;
            break;
        }
    }
    if(!rv) {
        SAH_TRACEZ_ERROR(ME, "Failed to convert '%s'", bbf_param_names);
    }

    return rv;
}

