
#pragma once

#include "ast/ast.h"
#include "util/lbool.h"

namespace user_propagator {

    class callback {
    public:
        virtual ~callback() = default;
        virtual bool propagate_cb(unsigned num_fixed, expr* const* fixed_ids, unsigned num_eqs, expr* const* eq_lhs, expr* const* eq_rhs, expr* conseq) = 0;
        virtual void register_cb(expr* e) = 0;
        virtual bool next_split_cb(expr* e, unsigned idx, lbool phase) = 0;
        // Queries existing Boolean state and the native trail length without registering the expression.
        virtual int assignment_value_cb(expr* atom, unsigned& prefix) {
            throw default_exception("assignment queries are only supported on the SMT solver");
        }
        // Resolves a signed semantic predicate to the Boolean identity used for native assignment callbacks.
        virtual expr_ref canonical_cb(expr* expression) {
            throw default_exception("canonical queries are only supported on the SMT solver");
        }
        // Recovers the complete reason of the assignment currently being reported.
        virtual void reason_cb(expr_ref_vector& result) {
            throw default_exception("assignment reasons are only supported on the SMT solver");
        }
    };
    
    class context_obj {
    public:
        virtual ~context_obj() = default;
    };
    
    typedef std::function<void(void*, callback*)>                            final_eh_t;
    typedef std::function<void(void*, callback*, expr*, expr*)>              fixed_eh_t;
    typedef std::function<void(void*, callback*, expr*, unsigned, unsigned)> assignment_eh_t;
    typedef std::function<void(void*, callback*, unsigned, expr* const*)> source_eh_t;
    typedef std::function<void(void*, callback*, expr*, expr*)>              eq_eh_t;
    typedef std::function<void*(void*, ast_manager&, context_obj*&)>         fresh_eh_t;
    typedef std::function<void(void*, callback*)>                            push_eh_t;
    typedef std::function<void(void*, callback*, unsigned)>                  pop_eh_t;
    typedef std::function<void(void*, callback*, expr*)>                     created_eh_t;
    typedef std::function<void(void*, callback*, expr*, unsigned, bool)>     decide_eh_t;
    typedef std::function<void(void*, expr*, unsigned, unsigned const*, unsigned, expr* const*)>        on_clause_eh_t;
    typedef std::function<bool(void*, callback*, expr*, expr*)>              binding_eh_t;

    class plugin : public decl_plugin {
    public:

        static symbol name() { return symbol("user_propagator"); }

        enum kind_t { OP_USER_PROPAGATE };

        decl_plugin* mk_fresh() override { return alloc(plugin); }

        sort* mk_sort(decl_kind k, unsigned num_parameters, parameter const* parameters) override {
            UNREACHABLE();
            return nullptr;
        }

        func_decl* mk_func_decl(decl_kind k, unsigned num_parameters, parameter const* parameters,
            unsigned arity, sort* const* domain, sort* range) override {
            UNREACHABLE();
            return nullptr;
        }

    };

    class core {
    public:

        virtual ~core() = default;

        // Registers actual Boolean trail notifications independently of observed theory terms.
        virtual void user_propagate_register_assignment(assignment_eh_t& handler) {
            throw default_exception("assignment reasons are only supported on the SMT solver");
        }
        
        // Registers query boundaries and complete learned clauses on the owning propagator instance.
        virtual void user_propagate_register_sources(source_eh_t& query, source_eh_t& learned) {
            throw default_exception("source callbacks are only supported on the SMT solver");
        }
        // Preserves the original query for source conditioning across preprocessing.
        virtual void user_propagate_query(expr_ref_vector const& conditions) {
            throw default_exception("original user-propagator queries are unsupported by this solver");
        }
        // Supplies explicit bit coordinates for semantic source and consequence translation.
        virtual void user_propagate_definitions(expr_ref_vector const& names, expr_ref_vector const& values) {
            throw default_exception("source definitions are only supported on the SMT solver");
        }

        virtual void user_propagate_init(
            void* ctx, 
            push_eh_t&                                   push_eh,
            pop_eh_t&                                    pop_eh,
            fresh_eh_t&                                  fresh_eh) {
            throw default_exception("user-propagators are only supported on the SMT solver");
        }        
        
        virtual void user_propagate_register_fixed(fixed_eh_t& fixed_eh) {
            throw default_exception("user-propagators are only supported on the SMT solver");
        }
        
        virtual void user_propagate_register_final(final_eh_t& final_eh) {
            throw default_exception("user-propagators are only supported on the SMT solver");
        }
        
        virtual void user_propagate_register_eq(eq_eh_t& eq_eh) {
            throw default_exception("user-propagators are only supported on the SMT solver");
        }
        
        virtual void user_propagate_register_diseq(eq_eh_t& diseq_eh) {
            throw default_exception("user-propagators are only supported on the SMT solver");
        }
        
        virtual void user_propagate_register_expr(expr* e) { 
            throw default_exception("user-propagators are only supported on the SMT solver");
        }

        virtual void user_propagate_register_created(created_eh_t& r) {
            throw default_exception("user-propagators are only supported on the SMT solver");
        }

        virtual void user_propagate_register_decide(decide_eh_t& r) {
            throw default_exception("user-propagators are only supported on the SMT solver");
        }

        virtual void user_propagate_register_on_binding(binding_eh_t& r) {
            throw default_exception("user-propagators are only supported on the SMT solver");
        }

        virtual void user_propagate_clear() {
        }

        virtual void register_on_clause(void*, on_clause_eh_t& r) { 
            throw default_exception("clause logging is only supported on the SMT solver");
        }

        virtual void user_propagate_initialize_value(expr* var, expr* value) {
            throw default_exception("value initialization is only supported on the SMT solver");            
        }

       
    };

}
