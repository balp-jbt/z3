/*++
Copyright (c) 2026

Module Name:

    spacer_profile.h

Abstract:

    Lightweight profiling support for SPACER.

--*/

#pragma once

#include <climits>
#include <fstream>
#include <string>
#include <vector>

#include "muz/base/dl_rule.h"
#include "util/lbool.h"
#include "util/obj_hashtable.h"
#include "util/vector.h"

namespace datalog {
class rule_set;
}

namespace spacer {

class lemma;
class pob;
class pred_transformer;

class spacer_profiler {
    struct tail_stats {
        unsigned m_selected_used = 0;
        unsigned m_selected_unused = 0;
        unsigned m_expanded_used = 0;
        unsigned m_blocked = 0;
        unsigned m_rule_origin_partial_candidates = 0;
    };

    struct rule_stats {
        datalog::rule const* m_rule = nullptr;
        unsigned m_id = UINT_MAX;
        std::string m_name;
        std::string m_head;
        std::vector<std::string> m_tails;
        unsigned m_interpreted_tail_count = 0;
        bool m_is_init = false;
        unsigned m_selected = 0;
        unsigned m_selected_concrete = 0;
        unsigned m_expanded = 0;
        unsigned m_child_create_success = 0;
        unsigned m_child_create_fail = 0;
        unsigned m_created_lemmas = 0;
        unsigned m_blocked = 0;
        double m_create_children_seconds = 0.0;
        std::vector<tail_stats> m_tail_stats;
    };

    std::string m_prefix;
    obj_map<datalog::rule const, unsigned> m_rule_ids;
    std::vector<rule_stats> m_rules;
    std::fstream m_pob_events;
    std::fstream m_propagation;
    unsigned m_next_event_id = 0;
    bool m_finished = false;

    // Purpose: Return a mutable stats record for an already registered rule.
    // Input: The rule pointer used by Spacer's transformed rule set.
    // Return: A stats pointer or null; this method has no side effects.
    rule_stats* find_rule(datalog::rule const& rule);

    // Purpose: Return a const stats record for an already registered rule.
    // Input: The rule pointer used by Spacer's transformed rule set.
    // Return: A stats pointer or null; this method has no side effects.
    rule_stats const* find_rule(datalog::rule const& rule) const;

    // Purpose: Return the stable profile id assigned to a rule.
    // Input: The rule pointer used by Spacer's transformed rule set.
    // Return: The rule id or UINT_MAX; this method has no side effects.
    unsigned get_rule_id(datalog::rule const* rule) const;

    // Purpose: Open a profile output file with the configured prefix.
    // Input: The suffix to append to the profile prefix.
    // Return: An output stream; throws if the file cannot be opened.
    std::fstream open_output(char const* suffix) const;

    // Purpose: Write the static rule table to disk.
    // Input: None; it reads the registered rule metadata.
    // Return: Nothing; writes the rules CSV as a side effect.
    void write_rules() const;

    // Purpose: Write aggregate rule counters to disk.
    // Input: None; it reads the accumulated rule statistics.
    // Return: Nothing; writes the rule stats CSV as a side effect.
    void write_rule_stats() const;

    // Purpose: Write aggregate per-rule-tail counters to disk.
    // Input: None; it reads the accumulated tail statistics.
    // Return: Nothing; writes the rule tail stats CSV as a side effect.
    void write_rule_tail_stats() const;

    // Purpose: Write the profiled PDG hypergraph to DOT.
    // Input: None; it reads registered rules and aggregate counters.
    // Return: Nothing; writes a DOT file as a side effect.
    void write_pdg_dot() const;

    // Purpose: Close streaming profile files.
    // Input: None; it uses the profiler's open file streams.
    // Return: Nothing; closes streams as a side effect.
    void close_streams();

public:
    // Purpose: Create a Spacer profiler rooted at an output file prefix.
    // Input: The output file prefix used for all profile artifacts.
    // Return: A profiler instance; opens streaming CSV files as a side effect.
    spacer_profiler(std::string const& prefix);

    // Purpose: Destroy a Spacer profiler.
    // Input: None; it uses the profiler's current stream state.
    // Return: Nothing; closes streams as a side effect.
    ~spacer_profiler();

    // Purpose: Register all transformed rules for static PDG profiling.
    // Input: The rule set currently installed in the Spacer context.
    // Return: Nothing; updates the rule-id table as a side effect.
    void register_rules(datalog::rule_set const& rules);

    // Purpose: Register one transformed rule for static PDG profiling.
    // Input: A rule from the current Spacer rule set.
    // Return: Nothing; assigns a rule id as a side effect if needed.
    void register_rule(datalog::rule const& rule);

    // Purpose: Record that reachability selected a rule from a model.
    // Input: The selected rule, concreteness flag, and used-tail bitmap.
    // Return: Nothing; increments aggregate counters as a side effect.
    void record_rule_selected(datalog::rule const& rule, bool is_concrete, bool_vector const& reach_pred_used);

    // Purpose: Record that Spacer expanded a rule into child obligations.
    // Input: The expanded rule and used-tail bitmap from reachability.
    // Return: Nothing; increments aggregate counters as a side effect.
    void record_rule_expanded(datalog::rule const& rule, bool_vector const& reach_pred_used);

    // Purpose: Record the final create-children outcome for a rule expansion.
    // Input: The expanded rule, success flag, and elapsed seconds.
    // Return: Nothing; increments aggregate counters as a side effect.
    void record_child_creation(datalog::rule const& rule, bool success, double seconds);

    // Purpose: Record a proof-obligation scheduling event.
    // Input: Event name, global level, POB, queue size, and result label.
    // Return: Nothing; appends one row to pob_events CSV as a side effect.
    void record_pob_event(char const* event, unsigned global_level, pob const& n, unsigned queue_size, char const* result);

    // Purpose: Record that a POB produced a new blocking lemma.
    // Input: The blocked proof obligation whose origin tags are inspected.
    // Return: Nothing; increments origin rule-tail counters as a side effect.
    void record_pob_blocked(pob const& n);

    // Purpose: Record one predicate-level lemma propagation attempt.
    // Input: Level, predicate transformer, lemma counters, and all-propagated flag.
    // Return: Nothing; appends one row to propagation CSV as a side effect.
    void record_propagation(unsigned level, pred_transformer const& pt, unsigned attempted, unsigned propagated, unsigned failed, bool all_propagated);

    // Purpose: Record a rule-origin partial-fixed candidate from a propagated lemma.
    // Input: The propagated lemma whose POB origin tags are inspected.
    // Return: Nothing; increments origin rule-tail counters as a side effect.
    void record_rule_origin_partial_candidate(lemma& lem);

    // Purpose: Finalize aggregate Spacer profile artifacts.
    // Input: The final Spacer query result.
    // Return: Nothing; writes aggregate files once as a side effect.
    void finish(lbool result);
};

}
