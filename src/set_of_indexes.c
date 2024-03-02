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

#include "set_of_indexes.h"

#include <stdlib.h> /* calloc(), free() */

#include <amxc/amxc_macros.h>

#include "mod_xpon_trace.h"

static void free_entry(amxc_llist_it_t* it) {
    index_entry_t* entry = amxc_container_of(it, index_entry_t, it);
    free(entry);
}

bool set_of_indexes_init(set_of_indexes_t* const set) {

    bool rv = false;
    when_null(set, exit);
    amxc_llist_init(&set->list);
    rv = true;

exit:
    return rv;
}


void set_of_indexes_clean(set_of_indexes_t* const set) {
    when_null(set, exit);

    amxc_llist_clean(&set->list, free_entry);

exit:
    return;
}

static bool does_set_have_index(const set_of_indexes_t* const set, uint32_t index) {

    bool rv = false;
    when_null(set, exit);

    index_entry_t* entry;
    amxc_llist_for_each(it, &set->list) {
        entry = amxc_container_of(it, index_entry_t, it);
        if(entry->index == index) {
            rv = true;
            break;
        }
    }

exit:
    return rv;
}

/**
 * Add index to set if it's not in the set yet.
 *
 * @param[in,out] set     set of indexes
 * @param[in] index       instance index to possibly add to @a set
 */
void set_of_indexes_add_index(set_of_indexes_t* const set, uint32_t index) {

    when_null(set, exit);

    if(does_set_have_index(set, index)) {
        return;
    }
    SAH_TRACEZ_DEBUG2(ME, "Add index = %d", index);
    index_entry_t* entry = (index_entry_t*) calloc(1, sizeof(index_entry_t));
    when_null_trace(entry, exit, ERROR, "Failed to allocate mem");
    entry->index = index;

    if(amxc_llist_append(&set->list, &entry->it)) {
        SAH_TRACEZ_ERROR(ME, "Failed to add index=%d", index);
        free(entry);
    }

exit:
    return;
}

/**
 * Return indexes in set as string formatted as comma-separated integers.
 *
 * @param[in] set          set of indexes
 * @param[in,out] indexes  the function returns the indexes in @a set as
 *                         string. See below for an example.
 *
 * Example:
 * If @a set has the integers 1, 2 and 5, the functions assigns the value
 * "1,2,5" to @a indexes.
 *
 * @return true on success, else false
 */
bool set_of_indexes_get_indexes_as_string(set_of_indexes_t* const set,
                                          amxc_string_t* const indexes) {

    bool rv = false;
    bool is_first = true;
    index_entry_t* entry;

    when_null(set, exit);
    when_null(indexes, exit);

    amxc_llist_for_each(it, &set->list) {
        entry = amxc_container_of(it, index_entry_t, it);
        if(is_first) {
            amxc_string_appendf(indexes, "%d", entry->index);
            is_first = false;
        } else {
            amxc_string_appendf(indexes, ",%d", entry->index);
        }
    }

    const char* const idxs = amxc_string_get(indexes, 0);
    SAH_TRACEZ_DEBUG(ME, "indexes='%s'", idxs ? idxs : "");
    rv = true;
exit:
    return rv;
}

