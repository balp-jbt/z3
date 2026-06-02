/*++
Copyright (c) 2026

Module Name:

    spacer_profile.cpp

Abstract:

    Lightweight profiling support for SPACER.

--*/

#include <sstream>

#include "muz/base/dl_rule_set.h"
#include "muz/spacer/spacer_context.h"
#include "muz/spacer/spacer_profile.h"
#include "util/z3_exception.h"

namespace spacer {

// Purpose: Convert a Boolean value to a stable CSV integer.
// Input: The Boolean value that should be serialized.
// Return: "1" for true and "0" for false; this function has no side effects.
static char const* csv_bool(bool value) {
    return value ? "1" : "0";
}

// Purpose: Escape one string for a CSV field.
// Input: The string that should be serialized as one field.
// Return: An escaped CSV string; this function has no side effects.
static std::string csv_escape(std::string const& text) {
    bool needs_quote = false;
    for (char ch : text) {
        if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r') {
            needs_quote = true;
            break;
        }
    }
    if (!needs_quote) {
        return text;
    }
    std::string result;
    result.push_back('"');
    for (char ch : text) {
        if (ch == '"') {
            result.push_back('"');
        }
        result.push_back(ch);
    }
    result.push_back('"');
    return result;
}

// Purpose: Escape one string for a DOT quoted id or label.
// Input: The string that should be serialized into a DOT file.
// Return: An escaped DOT string; this function has no side effects.
static std::string dot_quote(std::string const& text) {
    std::string result;
    result.push_back('"');
    for (char ch : text) {
        if (ch == '"' || ch == '\\') {
            result.push_back('\\');
        }
        if (ch == '\n' || ch == '\r') {
            result.push_back(' ');
        }
        else {
            result.push_back(ch);
        }
    }
    result.push_back('"');
    return result;
}

// Purpose: Convert a declaration name to a stable display string.
// Input: The function declaration whose name should be displayed.
// Return: The declaration name string; this function has no side effects.
static std::string decl_name(func_decl const* decl) {
    return decl ? decl->get_name().str() : "";
}

// Purpose: Convert a rule name to a stable display string.
// Input: The Datalog rule whose name should be displayed.
// Return: The rule name string; this function has no side effects.
static std::string rule_name(datalog::rule const& rule) {
    symbol const& name = rule.name();
    return name.is_null() ? "" : name.str();
}

// Purpose: Join tail predicate names into one CSV field.
// Input: The vector of tail predicate names.
// Return: A semicolon-separated string; this function has no side effects.
static std::string join_tails(std::vector<std::string> const& tails) {
    std::ostringstream out;
    for (unsigned i = 0; i < tails.size(); ++i) {
        if (i > 0) {
            out << ';';
        }
        out << tails[i];
    }
    return out.str();
}

// Purpose: Convert a Spacer result to a profile label.
// Input: The lbool result returned by the Spacer context.
// Return: A stable result string; this function has no side effects.
static char const* result_name(lbool result) {
    switch (result) {
    case l_true:
        return "sat";
    case l_false:
        return "unsat";
    case l_undef:
        return "unknown";
    }
    return "unknown";
}

// Purpose: Create a Spacer profiler rooted at an output file prefix.
// Input: The output file prefix used for all profile artifacts.
// Return: A profiler instance; opens streaming CSV files as a side effect.
spacer_profiler::spacer_profiler(std::string const& prefix) :
    m_prefix(prefix.empty() ? "spacer-profile" : prefix),
    m_pob_events(open_output(".pob_events.csv")),
    m_propagation(open_output(".propagation.csv")) {
    m_pob_events << "event_id,event,global_level,pob_level,pob_depth,predicate,post_id,parent_post_id,origin_rule_id,origin_tail_index,queue_size,result\n";
    m_propagation << "level,predicate,attempted_lemmas,propagated_lemmas,failed_lemmas,all_propagated,partial_fixed_predicate\n";
}

// Purpose: Destroy a Spacer profiler.
// Input: None; it uses the profiler's current stream state.
// Return: Nothing; closes streams as a side effect.
spacer_profiler::~spacer_profiler() {
    close_streams();
}

// Purpose: Return a mutable stats record for an already registered rule.
// Input: The rule pointer used by Spacer's transformed rule set.
// Return: A stats pointer or null; this method has no side effects.
spacer_profiler::rule_stats* spacer_profiler::find_rule(datalog::rule const& rule) {
    unsigned id = UINT_MAX;
    if (!m_rule_ids.find(&rule, id) || id >= m_rules.size()) {
        return nullptr;
    }
    return &m_rules[id];
}

// Purpose: Return a const stats record for an already registered rule.
// Input: The rule pointer used by Spacer's transformed rule set.
// Return: A stats pointer or null; this method has no side effects.
spacer_profiler::rule_stats const* spacer_profiler::find_rule(datalog::rule const& rule) const {
    unsigned id = UINT_MAX;
    if (!m_rule_ids.find(&rule, id) || id >= m_rules.size()) {
        return nullptr;
    }
    return &m_rules[id];
}

// Purpose: Return the stable profile id assigned to a rule.
// Input: The rule pointer used by Spacer's transformed rule set.
// Return: The rule id or UINT_MAX; this method has no side effects.
unsigned spacer_profiler::get_rule_id(datalog::rule const* rule) const {
    if (!rule) {
        return UINT_MAX;
    }
    unsigned id = UINT_MAX;
    return m_rule_ids.find(rule, id) ? id : UINT_MAX;
}

// Purpose: Open a profile output file with the configured prefix.
// Input: The suffix to append to the profile prefix.
// Return: An output stream; throws if the file cannot be opened.
std::fstream spacer_profiler::open_output(char const* suffix) const {
    std::string path = m_prefix + suffix;
    std::fstream out(path, std::ios_base::out);
    if (!out) {
        throw default_exception("could not open Spacer profile output file: " + path);
    }
    return out;
}

// Purpose: Register all transformed rules for static PDG profiling.
// Input: The rule set currently installed in the Spacer context.
// Return: Nothing; updates the rule-id table as a side effect.
void spacer_profiler::register_rules(datalog::rule_set const& rules) {
    for (datalog::rule* rule : rules) {
        register_rule(*rule);
    }
}

// Purpose: Register one transformed rule for static PDG profiling.
// Input: A rule from the current Spacer rule set.
// Return: Nothing; assigns a rule id as a side effect if needed.
void spacer_profiler::register_rule(datalog::rule const& rule) {
    if (m_rule_ids.contains(&rule)) {
        return;
    }

    rule_stats stats;
    stats.m_rule = &rule;
    stats.m_id = static_cast<unsigned>(m_rules.size());
    stats.m_name = rule_name(rule);
    stats.m_head = decl_name(rule.get_decl());
    unsigned tail_count = rule.get_uninterpreted_tail_size();
    stats.m_interpreted_tail_count = rule.get_tail_size() - tail_count;
    stats.m_is_init = tail_count == 0;
    for (unsigned i = 0; i < tail_count; ++i) {
        stats.m_tails.push_back(decl_name(rule.get_tail(i)->get_decl()));
        stats.m_tail_stats.push_back(tail_stats());
    }

    m_rule_ids.insert(&rule, stats.m_id);
    m_rules.push_back(stats);
}

// Purpose: Record that reachability selected a rule from a model.
// Input: The selected rule, concreteness flag, and used-tail bitmap.
// Return: Nothing; increments aggregate counters as a side effect.
void spacer_profiler::record_rule_selected(datalog::rule const& rule, bool is_concrete, bool_vector const& reach_pred_used) {
    rule_stats* stats = find_rule(rule);
    if (!stats) {
        register_rule(rule);
        stats = find_rule(rule);
    }
    if (!stats) {
        return;
    }
    ++stats->m_selected;
    if (is_concrete) {
        ++stats->m_selected_concrete;
    }
    for (unsigned i = 0; i < stats->m_tail_stats.size(); ++i) {
        bool used = i < reach_pred_used.size() && reach_pred_used[i];
        if (used) {
            ++stats->m_tail_stats[i].m_selected_used;
        }
        else {
            ++stats->m_tail_stats[i].m_selected_unused;
        }
    }
}

// Purpose: Record that Spacer expanded a rule into child obligations.
// Input: The expanded rule and used-tail bitmap from reachability.
// Return: Nothing; increments aggregate counters as a side effect.
void spacer_profiler::record_rule_expanded(datalog::rule const& rule, bool_vector const& reach_pred_used) {
    rule_stats* stats = find_rule(rule);
    if (!stats) {
        register_rule(rule);
        stats = find_rule(rule);
    }
    if (!stats) {
        return;
    }
    ++stats->m_expanded;
    for (unsigned i = 0; i < stats->m_tail_stats.size(); ++i) {
        bool used = i < reach_pred_used.size() && reach_pred_used[i];
        if (used) {
            ++stats->m_tail_stats[i].m_expanded_used;
        }
    }
}

// Purpose: Record the final create-children outcome for a rule expansion.
// Input: The expanded rule, success flag, and elapsed seconds.
// Return: Nothing; increments aggregate counters as a side effect.
void spacer_profiler::record_child_creation(datalog::rule const& rule, bool success, double seconds) {
    rule_stats* stats = find_rule(rule);
    if (!stats) {
        register_rule(rule);
        stats = find_rule(rule);
    }
    if (!stats) {
        return;
    }
    if (success) {
        ++stats->m_child_create_success;
    }
    else {
        ++stats->m_child_create_fail;
    }
    stats->m_create_children_seconds += seconds;
}

// Purpose: Record a proof-obligation scheduling event.
// Input: Event name, global level, POB, queue size, and result label.
// Return: Nothing; appends one row to pob_events CSV as a side effect.
void spacer_profiler::record_pob_event(char const* event, unsigned global_level, pob const& n, unsigned queue_size, char const* result) {
    std::string origin_rule_id;
    std::string origin_tail_index;
    if (n.profile_origin_rule()) {
        unsigned id = get_rule_id(n.profile_origin_rule());
        if (id != UINT_MAX) {
            origin_rule_id = std::to_string(id);
        }
        origin_tail_index = std::to_string(n.profile_origin_tail_index());
    }

    std::string parent_post_id;
    if (n.parent()) {
        parent_post_id = std::to_string(n.parent()->post()->get_id());
    }

    m_pob_events << m_next_event_id++ << ','
                 << csv_escape(event ? event : "") << ','
                 << global_level << ','
                 << n.level() << ','
                 << n.depth() << ','
                 << csv_escape(decl_name(n.pt().head())) << ','
                 << n.post()->get_id() << ','
                 << parent_post_id << ','
                 << origin_rule_id << ','
                 << origin_tail_index << ','
                 << queue_size << ','
                 << csv_escape(result ? result : "") << '\n';
}

// Purpose: Record that a POB produced a new blocking lemma.
// Input: The blocked proof obligation whose origin tags are inspected.
// Return: Nothing; increments origin rule-tail counters as a side effect.
void spacer_profiler::record_pob_blocked(pob const& n) {
    datalog::rule const* rule = n.profile_origin_rule();
    if (!rule) {
        return;
    }
    rule_stats* stats = find_rule(*rule);
    if (!stats) {
        return;
    }
    ++stats->m_created_lemmas;
    ++stats->m_blocked;
    unsigned tail_index = n.profile_origin_tail_index();
    if (tail_index < stats->m_tail_stats.size()) {
        ++stats->m_tail_stats[tail_index].m_blocked;
    }
}

// Purpose: Record one predicate-level lemma propagation attempt.
// Input: Level, predicate transformer, lemma counters, and all-propagated flag.
// Return: Nothing; appends one row to propagation CSV as a side effect.
void spacer_profiler::record_propagation(unsigned level, pred_transformer const& pt, unsigned attempted, unsigned propagated, unsigned failed, bool all_propagated) {
    bool partial_fixed = attempted > 0 && failed == 0;
    m_propagation << level << ','
                  << csv_escape(decl_name(pt.head())) << ','
                  << attempted << ','
                  << propagated << ','
                  << failed << ','
                  << csv_bool(all_propagated) << ','
                  << csv_bool(partial_fixed) << '\n';
}

// Purpose: Record a rule-origin partial-fixed candidate from a propagated lemma.
// Input: The propagated lemma whose POB origin tags are inspected.
// Return: Nothing; increments origin rule-tail counters as a side effect.
void spacer_profiler::record_rule_origin_partial_candidate(lemma& lem) {
    if (!lem.has_pob() || !lem.get_pob()) {
        return;
    }
    pob const& n = *lem.get_pob();
    datalog::rule const* rule = n.profile_origin_rule();
    if (!rule) {
        return;
    }
    rule_stats* stats = find_rule(*rule);
    if (!stats) {
        return;
    }
    unsigned tail_index = n.profile_origin_tail_index();
    if (tail_index < stats->m_tail_stats.size()) {
        ++stats->m_tail_stats[tail_index].m_rule_origin_partial_candidates;
    }
}

// Purpose: Write the static rule table to disk.
// Input: None; it reads the registered rule metadata.
// Return: Nothing; writes the rules CSV as a side effect.
void spacer_profiler::write_rules() const {
    std::fstream out = open_output(".rules.csv");
    out << "rule_id,rule_name,head_pred,tail_count,tail_preds,interpreted_tail_count,is_init\n";
    for (rule_stats const& stats : m_rules) {
        out << stats.m_id << ','
            << csv_escape(stats.m_name) << ','
            << csv_escape(stats.m_head) << ','
            << stats.m_tails.size() << ','
            << csv_escape(join_tails(stats.m_tails)) << ','
            << stats.m_interpreted_tail_count << ','
            << csv_bool(stats.m_is_init) << '\n';
    }
}

// Purpose: Write aggregate rule counters to disk.
// Input: None; it reads the accumulated rule statistics.
// Return: Nothing; writes the rule stats CSV as a side effect.
void spacer_profiler::write_rule_stats() const {
    std::fstream out = open_output(".rule_stats.csv");
    out << "rule_id,rule_name,head_pred,selected_count,selected_concrete_count,expanded_count,child_create_success,child_create_fail,created_lemmas,block_count,create_children_seconds\n";
    for (rule_stats const& stats : m_rules) {
        out << stats.m_id << ','
            << csv_escape(stats.m_name) << ','
            << csv_escape(stats.m_head) << ','
            << stats.m_selected << ','
            << stats.m_selected_concrete << ','
            << stats.m_expanded << ','
            << stats.m_child_create_success << ','
            << stats.m_child_create_fail << ','
            << stats.m_created_lemmas << ','
            << stats.m_blocked << ','
            << stats.m_create_children_seconds << '\n';
    }
}

// Purpose: Write aggregate per-rule-tail counters to disk.
// Input: None; it reads the accumulated tail statistics.
// Return: Nothing; writes the rule tail stats CSV as a side effect.
void spacer_profiler::write_rule_tail_stats() const {
    std::fstream out = open_output(".rule_tail_stats.csv");
    out << "rule_id,tail_index,tail_pred,selected_used_count,selected_unused_count,expanded_used_count,block_count,rule_origin_partial_candidates\n";
    for (rule_stats const& stats : m_rules) {
        for (unsigned i = 0; i < stats.m_tail_stats.size(); ++i) {
            tail_stats const& tail = stats.m_tail_stats[i];
            out << stats.m_id << ','
                << i << ','
                << csv_escape(stats.m_tails[i]) << ','
                << tail.m_selected_used << ','
                << tail.m_selected_unused << ','
                << tail.m_expanded_used << ','
                << tail.m_blocked << ','
                << tail.m_rule_origin_partial_candidates << '\n';
        }
    }
}

// Purpose: Write the profiled PDG hypergraph to DOT.
// Input: None; it reads registered rules and aggregate counters.
// Return: Nothing; writes a DOT file as a side effect.
void spacer_profiler::write_pdg_dot() const {
    std::fstream out = open_output(".pdg.dot");
    out << "digraph spacer_pdg {\n";
    out << "  rankdir=LR;\n";
    for (rule_stats const& stats : m_rules) {
        std::string rule_node = "rule:" + std::to_string(stats.m_id);
        std::ostringstream label;
        label << "r" << stats.m_id
              << "\\nsel=" << stats.m_selected
              << "\\nexp=" << stats.m_expanded
              << "\\nblk=" << stats.m_blocked;
        out << "  " << dot_quote(rule_node)
            << " [shape=box,label=" << dot_quote(label.str()) << "];\n";
        out << "  " << dot_quote("pred:" + stats.m_head)
            << " [shape=ellipse,label=" << dot_quote(stats.m_head) << "];\n";
        for (std::string const& tail : stats.m_tails) {
            out << "  " << dot_quote("pred:" + tail)
                << " [shape=ellipse,label=" << dot_quote(tail) << "];\n";
            out << "  " << dot_quote("pred:" + tail)
                << " -> " << dot_quote(rule_node) << ";\n";
        }
        out << "  " << dot_quote(rule_node)
            << " -> " << dot_quote("pred:" + stats.m_head) << ";\n";
    }
    out << "}\n";
}

// Purpose: Finalize aggregate Spacer profile artifacts.
// Input: The final Spacer query result.
// Return: Nothing; writes aggregate files once as a side effect.
void spacer_profiler::finish(lbool result) {
    if (m_finished) {
        return;
    }
    m_finished = true;
    m_pob_events.flush();
    m_propagation.flush();
    write_rules();
    write_rule_stats();
    write_rule_tail_stats();
    write_pdg_dot();

    std::fstream out = open_output(".summary.csv");
    out << "result,rule_count,pob_event_count\n";
    out << result_name(result) << ','
        << m_rules.size() << ','
        << m_next_event_id << '\n';
}

// Purpose: Close streaming profile files.
// Input: None; it uses the profiler's open file streams.
// Return: Nothing; closes streams as a side effect.
void spacer_profiler::close_streams() {
    if (m_pob_events.is_open()) {
        m_pob_events.close();
    }
    if (m_propagation.is_open()) {
        m_propagation.close();
    }
}

}
