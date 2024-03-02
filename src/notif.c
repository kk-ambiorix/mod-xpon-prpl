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

#include "notif.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>           /* STDOUT_FILENO */

#include <amxc/amxc_macros.h> /* UNUSED */
#include <amxb/amxb_subscribe.h>

#include "dm_info.h"           /* dm_convert_prpl_path_to_bbf_path() */
#include "mod_xpon_macros.h"   /* ARRAY_SIZE() */
#include "mod_xpon_trace.h"
#include "object_utils.h"      /* obj_process_object_params() */
#include "southbound_if.h"     /* sbi_query_object() */
#include "xpon_mgr_pon_stat.h" /* xpon_mngr_call_pon_stat_function() */

static amxb_bus_ctx_t* s_bus_ctx = NULL;

typedef void (* handle_notification_fn_t) (uint32_t onu_index, const amxc_var_t* const data);

typedef enum _dm_notification {
    notif_dm_instance_added = 0,
    notif_dm_instance_removed,
    notif_dm_object_changed,
    notif_nr
} dm_notification_t;

typedef struct _subscription_info {
    uint32_t onu_index;
    bool subscribed;
} subscription_info_t;

static subscription_info_t s_subscription_info[MAX_NR_OF_ONUS];

static const char* dm_notification_to_xpon_mgr_func_name(dm_notification_t notif) {

    switch(notif) {
    case notif_dm_instance_added:   return "dm_instance_added";
    case notif_dm_instance_removed: return "dm_instance_removed";
    case notif_dm_object_changed:   return "dm_object_changed";
    default: break;
    }
    return NULL;
}

/**
 * Handle a notification.
 *
 * @param[in] notif   notification type
 * @param[in] data    variant with info the ONU HAL agent sent with the
 *                    notification. All notifications handled by this function
 *                    must at least include a value for 'path'.
 *
 * In short, collect info needed and call function in 'pon_stat' namespace of
 * the tr181-xpon plugin passing the info as argument.
 *
 * The function creates a variant with an htable. Depending on the notification
 * type, it adds values for following keys to the htable:
 * - notif_dm_instance_added: 'path', 'index', 'keys', 'parameters'
 * - notif_dm_instance_removed: 'path', 'index'
 * - notif_dm_object_changed: 'path', 'parameters'
 *
 * Because the ONU HAL agent only sends values for 'path' and 'index', the
 * function does not have all the info it needs for the notification types
 * notif_dm_instance_added and notif_dm_object_changed. For those ones the
 * function calls get() on the instance added or the object changed to get this
 * info.
 *
 * Then it calls the function in the 'pon_stat' namespace of the tr181-xpon
 * plugin corresponding to the notification type, passing the variant as
 * argument.
 */
static void handle_dm_notification(dm_notification_t notif,
                                   const amxc_var_t* const data) {

    when_null_trace(data, exit, ERROR, "data is NULL");
    when_null_trace(s_bus_ctx, exit, ERROR, "No bus context");
    when_false_trace(notif < notif_nr, exit, ERROR, "Invalid value for 'notif'");

    const char* const path = GETP_CHAR(data, "path");
    const uint32_t index = GETP_UINT32(data, "index");

    when_null_trace(path, exit, ERROR, "notification does not have value for 'path'");

    if((notif_dm_instance_added == notif) ||
       (notif_dm_instance_removed == notif)) {
        if(0 == index) {
            SAH_TRACEZ_ERROR(ME, "notification does not include (valid) value for 'index'");
            goto exit;
        }
    }
    SAH_TRACEZ_DEBUG(ME, "path='%s' index=%d", path, index);


    amxc_string_t bbf_path;
    amxc_string_t prpl_path;
    amxc_var_t params; /* params of the prpl object */
    /* args for dm_instance_added(), dm_instance_removed() or dm_object_changed() call */
    amxc_var_t args;

    amxc_string_init(&bbf_path, 0);
    amxc_string_init(&prpl_path, 0);
    amxc_var_init(&params);
    amxc_var_init(&args);

    if(!dm_convert_prpl_path_to_bbf_path(path, &bbf_path)) {
        SAH_TRACEZ_ERROR(ME, "Failed to convert '%s' to bbf path", path);
        goto exit_clean;
    }

    if((notif_dm_instance_added == notif) ||
       (notif_dm_object_changed == notif)) {

        const object_id_t id = dm_get_object_id(path);
        if(obj_id_unknown == id) {
            SAH_TRACEZ_ERROR(ME, "Failed to get ID for path '%s'", path);
            goto exit_clean;
        }

        amxc_string_set(&prpl_path, path);
        if(notif_dm_instance_added == notif) {
            amxc_string_appendf(&prpl_path, ".%d", index);
        }

        if(!sbi_query_object(s_bus_ctx, &prpl_path, &params)) {
            SAH_TRACEZ_ERROR(ME, "Failed to query %s", amxc_string_get(&prpl_path, 0));
            goto exit_clean;
        }

        const bool extract_key = (notif_dm_instance_added == notif);

        if(!obj_process_object_params(id, &params, extract_key, &args,
                                      amxc_string_get(&prpl_path, 0))) {
            goto exit_clean;
        }
    } else {
        amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    }

    if(!amxc_var_add_key(cstring_t, &args, "path", amxc_string_get(&bbf_path, 0))) {
        SAH_TRACEZ_ERROR(ME, "Failed to add path to args");
        goto exit_clean;
    }

    if(index != 0) {
        if(!amxc_var_add_key(uint32_t, &args, "index", index)) {
            SAH_TRACEZ_ERROR(ME, "Failed to add index to args");
            goto exit_clean;
        }
    }

    /* Function name to call towards XPON manager */
    const char* const func_name = dm_notification_to_xpon_mgr_func_name(notif);
    xpon_mngr_call_pon_stat_function(func_name, &args);

exit_clean:
    amxc_string_clean(&bbf_path);
    amxc_string_clean(&prpl_path);
    amxc_var_clean(&params);
    amxc_var_clean(&args);

exit:
    return;
}


/**
 * Call 'dm_instance_added()' in tr181-xpon plugin.
 *
 * @param[in] data   should contain the 'path' and the 'index' of the instance added
 */
static void handle_dm_instance_added(UNUSED uint32_t onu_index, const amxc_var_t* const data) {

    handle_dm_notification(notif_dm_instance_added, data);
}

/**
 * Call 'dm_instance_removed()' in tr181-xpon plugin.
 *
 * @param[in] data   should contain the 'path' and the 'index' of the instance removed
 */
static void handle_dm_instance_removed(UNUSED uint32_t onu_index, const amxc_var_t* const data) {

    handle_dm_notification(notif_dm_instance_removed, data);
}

/**
 * Call 'dm_object_changed()' in tr181-xpon plugin.
 *
 * @param[in] data   should contain the 'path' of the object changed
 */
static void handle_dm_object_changed(UNUSED uint32_t onu_index, const amxc_var_t* const data) {

    handle_dm_notification(notif_dm_object_changed, data);
}

/**
 * Call 'omci_reset_mib()' in tr181-xpon plugin.
 *
 * @param[in] onu_index   xpon_onu instance index
 *
 * Create a htable with 1 element with key="index" and @a onu_index as value.
 * Pass the htable as argument of omci_reset_mib().
 */
static void handle_omci_reset_mib(uint32_t onu_index, UNUSED const amxc_var_t* const data) {

    amxc_var_t args;
    amxc_var_init(&args);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);

    if(!amxc_var_add_key(uint32_t, &args, "index", onu_index)) {
        SAH_TRACEZ_ERROR(ME, "Failed to add index to args");
        goto exit;
    }
    xpon_mngr_call_pon_stat_function("omci_reset_mib", &args);

exit:
    amxc_var_clean(&args);
}


typedef struct _notification_handler {
    const char* name; /* notification name, e.g., "dm:instance-added" */
    handle_notification_fn_t handler;
} notification_handler_t;

static const notification_handler_t NOTIFICATION_HANDLERS[] = {
    { .name = "dm:instance-added", .handler = handle_dm_instance_added   },
    { .name = "dm:instance-removed", .handler = handle_dm_instance_removed },
    { .name = "dm:object-changed", .handler = handle_dm_object_changed   },
    { .name = "omci:reset_mib", .handler = handle_omci_reset_mib      }
};

/**
 * Notification handler.
 *
 * @param[in] data  contains the notification name and info relevant for that
 *                  notification. E.g., for the "dm:instance-added" notification
 *                  @a data should include values for 'path' and 'index' to
 *                  indicate which instance is added.
 * @param[in] priv  pointer to subscription_info_t instance. Its field
 *                  'onu_index' shows which xpon_onu instance sent the
 *                  notification.
 */
static void notif_handler(const char* const sig_name,
                          const amxc_var_t* const data,
                          void* const priv) {

    SAH_TRACEZ_DEBUG(ME, "sig_name='%s'", sig_name);

    when_null_trace(data, exit, ERROR, "data is NULL");
    when_null_trace(priv, exit, ERROR, "priv is NULL");

#ifdef _DEBUG_
    printf("notif_handler(): data:\n");
    amxc_var_dump(data, STDOUT_FILENO);
#endif

    const char* const notification = GETP_CHAR(data, "notification");
    when_null_trace(notification, exit, ERROR, "Notification does not include name");

    const subscription_info_t* info = (const subscription_info_t*) priv;
    SAH_TRACEZ_DEBUG(ME, "onu_index=%d: notification='%s'", info->onu_index, notification);
    if(!info->subscribed) {
        SAH_TRACEZ_WARNING(ME, "onu_index=%d: ignore '%s': not subscribed",
                           info->onu_index, notification);
        goto exit;
    }

    const size_t n_notifs = ARRAY_SIZE(NOTIFICATION_HANDLERS);
    size_t i;

    bool found = false;
    for(i = 0; i < n_notifs; ++i) {
        if(strncmp(notification, NOTIFICATION_HANDLERS[i].name,
                   strlen(NOTIFICATION_HANDLERS[i].name)) == 0) {
            NOTIFICATION_HANDLERS[i].handler(info->onu_index, data);
            found = true;
            break;
        }
    }

    if(!found) {
        SAH_TRACEZ_WARNING(ME, "Unknown notification: %s", notification);
    }

exit:
    return;
}

/**
 * Initialize the 'notif' part.
 *
 * The module must call this function once at startup.
 */
void notif_init(void) {

    int i;
    for(i = 0; i < MAX_NR_OF_ONUS; ++i) {
        s_subscription_info[i].onu_index = (i + 1);
        s_subscription_info[i].subscribed = false;
    }
}

static inline bool is_valid_index(uint32_t index) {

    return ((index == 0) || (index > MAX_NR_OF_ONUS)) ? false : true;
}

/**
 * Return true if plugin is subscribed on notifications from xpon_onu.<index>
 *
 * @param[in] index   xpon_onu instance index. Must be in interval [1, MAX_NR_OF_ONUS].
 *
 * @return true if plugin is subscribed on notifications from xpon_onu instance, else false
 */
bool notif_is_subscribed(uint32_t index) {

    when_false_trace(is_valid_index(index), exit_error, ERROR, "Invalid index [%d]", index);

    return s_subscription_info[index - 1].subscribed;

exit_error:
    return false;
}

/**
 * Subscribe on notifications from xpon_onu.<index>
 *
 * @param[in] ctx    bus context
 * @param[in] index  xpon_onu instance index. Must be in interval [1, MAX_NR_OF_ONUS].
 *
 * When subscribing, pass pointer to entry in s_subscription_info array
 * corresponding to @a index. If the plugin calls the callback function upon
 * receiving a notification, the plugin can find out to which xpon_onu instance
 * the notification belongs.
 */
void notif_subscribe(amxb_bus_ctx_t* const ctx, uint32_t index) {

    when_null_trace(ctx, exit, ERROR, "bus context is NULL");
    when_false_trace(is_valid_index(index), exit, ERROR, "Invalid index [%d]", index);

    SAH_TRACEZ_DEBUG(ME, "index='%d'", index);
    const uint32_t idx = index - 1;

    if(s_subscription_info[idx].subscribed) {
        SAH_TRACEZ_DEBUG(ME, "Already subscribed on xpon_onu.%d", index);
        goto exit;
    }

    if(s_bus_ctx) {
        if(s_bus_ctx != ctx) {
            /* We should normally never end up here */
            SAH_TRACEZ_ERROR(ME, "Bus context changed (index=%d)", index);
            goto exit;
        }
    } else {
        s_bus_ctx = ctx;
    }

    char object[16];

    /* amxb_subscribe() expects the parameter 'object' end with a "." */
    snprintf(object, 16, "xpon_onu.%d.", index);

    if(amxb_subscribe(ctx, object, NULL, notif_handler, &s_subscription_info[idx])) {
        SAH_TRACEZ_ERROR(ME, "Failed to subscribe on %s", object);
    } else {
        s_subscription_info[idx].subscribed = true;
    }

exit:
    return;
}

/**
 * Clean up the notif part.
 *
 * Unsubscribe from all xpon_oni.{i} instances.
 *
 * The module must call this function once when stopping.
 */
void notif_cleanup(void) {
    uint32_t i;
    if(s_bus_ctx) {
        char object[16];
        for(i = 0; i < MAX_NR_OF_ONUS; ++i) {
            if(s_subscription_info[i].subscribed) {
                /* amxb_unsubscribe() expects the parameter 'object' end with a "." */
                snprintf(object, 16, "xpon_onu.%d.", i + 1);
                SAH_TRACEZ_DEBUG(ME, "%s: unsubscribe", object);
                if(amxb_unsubscribe(s_bus_ctx, object, notif_handler,
                                    &s_subscription_info[i])) {
                    SAH_TRACEZ_ERROR(ME, "Failed to unsubscribe from %s", object);
                } else {
                    s_subscription_info[i].subscribed = false;
                }
            }
        }
    }
}

