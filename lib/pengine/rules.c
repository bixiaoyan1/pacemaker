/*
 * Copyright 2004-2024 the Pacemaker project contributors
 *
 * The version control history for this file may have further details.
 *
 * This source code is licensed under the GNU Lesser General Public License
 * version 2.1 or later (LGPLv2.1+) WITHOUT ANY WARRANTY.
 */

#include <crm_internal.h>

#include <glib.h>

#include <crm/crm.h>
#include <crm/common/xml.h>
#include <crm/pengine/rules.h>

#include <crm/common/iso8601_internal.h>
#include <crm/common/nvpair_internal.h>
#include <crm/common/rules_internal.h>
#include <crm/common/xml_internal.h>
#include <crm/pengine/internal.h>
#include <crm/pengine/rules_internal.h>

#include <sys/types.h>
#include <regex.h>

CRM_TRACE_INIT_DATA(pe_rules);

/*!
 * \internal
 * \brief Map pe_rule_eval_data_t to pcmk_rule_input_t
 *
 * \param[out] new  New data struct
 * \param[in]  old  Old data struct
 */
static void
map_rule_input(pcmk_rule_input_t *new, const pe_rule_eval_data_t *old)
{
    if (old == NULL) {
        return;
    }
    new->now = old->now;
    new->node_attrs = old->node_hash;
    if (old->rsc_data != NULL) {
        new->rsc_standard = old->rsc_data->standard;
        new->rsc_provider = old->rsc_data->provider;
        new->rsc_agent = old->rsc_data->agent;
    }
    if (old->match_data != NULL) {
        new->rsc_params = old->match_data->params;
        new->rsc_meta = old->match_data->meta;
        if (old->match_data->re != NULL) {
            new->rsc_id = old->match_data->re->string;
            new->rsc_id_submatches = old->match_data->re->pmatch;
            new->rsc_id_nmatches = old->match_data->re->nregs;
        }
    }
    if (old->op_data != NULL) {
        new->op_name = old->op_data->op_name;
        new->op_interval_ms = old->op_data->interval;
    }
}

/*!
 * \internal
 * \brief Unpack a single nvpair XML element into a hash table
 *
 * \param[in]     nvpair    XML nvpair element to unpack
 * \param[in,out] userdata  Unpack data
 *
 * \return pcmk_rc_ok (to always proceed to next nvpair)
 */
static int
unpack_nvpair(xmlNode *nvpair, void *userdata)
{
    pcmk__nvpair_unpack_t *unpack_data = userdata;

    const char *name = NULL;
    const char *value = NULL;
    const char *old_value = NULL;
    const xmlNode *ref_nvpair = pcmk__xe_resolve_idref(nvpair, NULL);

    if (ref_nvpair == NULL) {
        /* Not possible with schema validation enabled (error already
         * logged)
         */
        return pcmk_rc_ok;
    }

    name = crm_element_value(ref_nvpair, PCMK_XA_NAME);
    value = crm_element_value(ref_nvpair, PCMK_XA_VALUE);
    if ((name == NULL) || (value == NULL)) {
        return pcmk_rc_ok; // Not possible with schema validation enabled
    }

    old_value = g_hash_table_lookup(unpack_data->values, name);

    if (pcmk__str_eq(value, "#default", pcmk__str_casei)) {
        // @COMPAT Deprecated since 2.1.8
        pcmk__config_warn("Support for setting meta-attributes (such as "
                          "%s) to the explicit value '#default' is "
                          "deprecated and will be removed in a future "
                          "release", name);
        if (old_value != NULL) {
            crm_trace("Letting %s default (removing explicit value \"%s\")",
                      name, value);
            g_hash_table_remove(unpack_data->values, name);
        }

    } else if ((old_value == NULL) || unpack_data->overwrite) {
        crm_trace("Setting %s=\"%s\" (was %s)",
                  name, value, pcmk__s(old_value, "unset"));
        pcmk__insert_dup(unpack_data->values, name, value);
    }
    return pcmk_rc_ok;
}

static void
unpack_attr_set(gpointer data, gpointer user_data)
{
    xmlNode *pair = data;
    pcmk__nvpair_unpack_t *unpack_data = user_data;

    xmlNode *rule_xml = pcmk__xe_first_child(pair, PCMK_XE_RULE, NULL, NULL);

    if ((rule_xml != NULL)
        && (pcmk_evaluate_rule(rule_xml, &(unpack_data->rule_input),
                               unpack_data->next_change) != pcmk_rc_ok)) {
        return;
    }

    crm_trace("Adding name/value pairs from %s %s overwrite",
              pcmk__xe_id(pair), (unpack_data->overwrite? "with" : "without"));
    if (pcmk__xe_is(pair->children, PCMK__XE_ATTRIBUTES)) {
        pair = pair->children;
    }
    pcmk__xe_foreach_child(pair, PCMK_XE_NVPAIR, unpack_nvpair, unpack_data);
}

/*!
 * \brief Extract nvpair blocks contained by an XML element into a hash table
 *
 * \param[in,out] top           Ignored
 * \param[in]     xml_obj       XML element containing blocks of nvpair elements
 * \param[in]     set_name      If not NULL, only use blocks of this element
 * \param[in]     rule_data     Matching parameters to use when unpacking
 * \param[out]    hash          Where to store extracted name/value pairs
 * \param[in]     always_first  If not NULL, process block with this ID first
 * \param[in]     overwrite     Whether to replace existing values with same
 *                              name (all internal callers pass \c FALSE)
 * \param[out]    next_change   If not NULL, set to when evaluation will change
 */
void
pe_eval_nvpairs(xmlNode *top, const xmlNode *xml_obj, const char *set_name,
                const pe_rule_eval_data_t *rule_data, GHashTable *hash,
                const char *always_first, gboolean overwrite,
                crm_time_t *next_change)
{
    GList *pairs = pcmk__xe_dereference_children(xml_obj, set_name);

    if (pairs) {
        pcmk__nvpair_unpack_t data = {
            .values = hash,
            .first_id = always_first,
            .overwrite = overwrite,
            .next_change = next_change,
        };

        map_rule_input(&(data.rule_input), rule_data);

        pairs = g_list_sort_with_data(pairs, pcmk__cmp_nvpair_blocks, &data);
        g_list_foreach(pairs, unpack_attr_set, &data);
        g_list_free(pairs);
    }
}

/*!
 * \brief Extract nvpair blocks contained by an XML element into a hash table
 *
 * \param[in,out] top           Ignored
 * \param[in]     xml_obj       XML element containing blocks of nvpair elements
 * \param[in]     set_name      Element name to identify nvpair blocks
 * \param[in]     node_hash     Node attributes to use when evaluating rules
 * \param[out]    hash          Where to store extracted name/value pairs
 * \param[in]     always_first  If not NULL, process block with this ID first
 * \param[in]     overwrite     Whether to replace existing values with same
 *                              name (all internal callers pass \c FALSE)
 * \param[in]     now           Time to use when evaluating rules
 * \param[out]    next_change   If not NULL, set to when evaluation will change
 */
void
pe_unpack_nvpairs(xmlNode *top, const xmlNode *xml_obj, const char *set_name,
                  GHashTable *node_hash, GHashTable *hash,
                  const char *always_first, gboolean overwrite,
                  crm_time_t *now, crm_time_t *next_change)
{
    pe_rule_eval_data_t rule_data = {
        .node_hash = node_hash,
        .now = now,
        .match_data = NULL,
        .rsc_data = NULL,
        .op_data = NULL
    };

    pe_eval_nvpairs(NULL, xml_obj, set_name, &rule_data, hash,
                    always_first, overwrite, next_change);
}

// Deprecated functions kept only for backward API compatibility
// LCOV_EXCL_START

#include <crm/pengine/rules_compat.h>

gboolean
test_rule(xmlNode * rule, GHashTable * node_hash, enum rsc_role_e role, crm_time_t * now)
{
    pcmk_rule_input_t rule_input = {
        .node_attrs = node_hash,
        .now = now,
    };

    return pcmk_evaluate_rule(rule, &rule_input, NULL) == pcmk_rc_ok;
}

// LCOV_EXCL_STOP
// End deprecated API
