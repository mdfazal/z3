/*++
  Copyright (c) 2019 Microsoft Corporation

  Module Name:

     emonomials.h

  Abstract:

     table that associate monomials to congruence class representatives modulo a union find structure.

  Author:
    Nikolaj Bjorner (nbjorner)
    Lev Nachmanson (levnach)

  Revision History:

    to replace rooted_mons.h and rooted_mon, rooted_mon_tabled

  --*/

#pragma once
#include "util/lp/lp_utils.h"
#include "util/lp/var_eqs.h"
#include "util/lp/monomial.h"
#include "util/region.h"

namespace nla {

    /**
       \brief class used to summarize the coefficients to a monomial after
       canonization with respect to current equalities.
    */
    class signed_vars {
        lpvar           m_var; // variable representing original monomial
        svector<lpvar>  m_vars;
        bool            m_sign;
    public:
        signed_vars(lpvar v) : m_var(v), m_sign(false) {}
        lpvar var() const { return m_var; }
        svector<lpvar> const& vars() const { return m_vars; }
        svector<lp::var_index>::const_iterator begin() const { return vars().begin(); }
        svector<lp::var_index>::const_iterator end() const { return vars().end(); }
        unsigned size() const { return m_vars.size(); }
        lpvar operator[](unsigned i) const { return m_vars[i]; }
        bool sign() const { return m_sign; }
        rational rsign() const { return rational(m_sign ? -1 : 1); }
        void reset() { m_sign = false; m_vars.reset(); }
        void push_var(signed_var sv) { m_sign ^= sv.sign(); m_vars.push_back(sv.var()); }
        void done_push() {
            std::sort(m_vars.begin(), m_vars.end());
        }
        std::ostream& display(std::ostream& out) const {
            out << "v" << var() << " := ";
            if (sign()) out << "- ";
            for (lpvar v : vars()) out << "v" << v << " ";
            return out;
        }
    };

    inline std::ostream& operator<<(std::ostream& out, signed_vars const& m) { return m.display(out); }


    class emonomials : public var_eqs_merge_handler {

        /**
           \brief singly-lined cyclic list of monomial indices where variable occurs.
           Each variable points to the head and tail of the cyclic list.
           Initially, head and tail are nullptr.
           New elements are inserted in the beginning of the list.
           Two lists are merged when equivalence class representatives are merged, 
           and the merge is undone when the representative variables are unmerged.
         */
        struct cell {
            cell(unsigned mIndex, cell* c): m_next(c), m_index(mIndex) {}
            cell*        m_next;
            unsigned     m_index;
        };
        struct head_tail {
            head_tail(): m_head(nullptr), m_tail(nullptr) {}
            cell* m_head;
            cell* m_tail;
        };


        /**
           \brief private fields used by emonomials for maintaining state of canonized monomials.
        */
        class signed_vars_ts : public signed_vars {
        public:
            signed_vars_ts(lpvar v, unsigned idx): signed_vars(v), m_next(idx), m_prev(idx), m_visited(0) {}
            unsigned m_next;                    // next congruent node.
            unsigned m_prev;                    // previous congruent node
            mutable unsigned m_visited;
        };

        struct hash_canonical {
            emonomials& em;
            hash_canonical(emonomials& em): em(em) {}

            unsigned operator()(lpvar v) const {
                auto const& vec = em.m_canonized[em.m_var2index[v]].vars();
                return string_hash(reinterpret_cast<char const*>(vec.c_ptr()), sizeof(lpvar)*vec.size(), 10);
            }
        };

        struct eq_canonical {
            emonomials& em;
            eq_canonical(emonomials& em): em(em) {}
            bool operator()(lpvar u, lpvar v) const {
                auto const& uvec = em.m_canonized[em.m_var2index[u]].vars();
                auto const& vvec = em.m_canonized[em.m_var2index[v]].vars();
                return uvec == vvec;
            }
        };

        var_eqs&                        m_ve;
        mutable vector<monomial>        m_monomials;     // set of monomials
        mutable unsigned_vector         m_var2index;     // var_mIndex -> mIndex
        unsigned_vector                 m_lim;           // backtracking point
        mutable unsigned                m_visited;       // timestamp of visited monomials during pf_iterator
        region                          m_region;        // region for allocating linked lists
        mutable vector<signed_vars_ts>  m_canonized;     // canonized versions of signed variables
        mutable svector<head_tail>      m_use_lists;     // use list of monomials where variables occur.
        hash_canonical                  m_cg_hash;
        eq_canonical                    m_cg_eq;
        hashtable<lpvar, hash_canonical, eq_canonical> m_cg_table; // congruence (canonical) table.

        void inc_visited() const;

        void remove_cell(head_tail& v, unsigned mIndex);
        void insert_cell(head_tail& v, unsigned mIndex);
        void merge_cells(head_tail& root, head_tail& other);
        void unmerge_cells(head_tail& root, head_tail& other);

        void remove_cg(lpvar v);
        void insert_cg(lpvar v);
        void insert_cg(unsigned idx, monomial const& m);
        void remove_cg(unsigned idx, monomial const& m);
        void rehash_cg(lpvar v) { remove_cg(v); insert_cg(v); }

        void do_canonize(monomial const& m) const; 

        cell* head(lpvar v) const;
        void set_visited(monomial const& m) const;
        bool is_visited(monomial const& m) const;
        
    public:
        /**
           \brief emonomials builds on top of var_eqs.
           push and pop on emonomials calls push/pop on var_eqs, so no 
           other calls to push/pop to the var_eqs should take place. 
         */
        emonomials(var_eqs& ve): 
            m_ve(ve), 
            m_visited(0), 
            m_cg_hash(*this),
            m_cg_eq(*this),
            m_cg_table(DEFAULT_HASHTABLE_INITIAL_CAPACITY, m_cg_hash, m_cg_eq),
            canonical(*this) { 
            m_ve.set_merge_handler(this); 
        }

        /**
           \brief push/pop scopes. 
           The life-time of a merge is local within a scope.
        */
        void push();

        void pop(unsigned n);

        /**
           \brief create a monomial from an equality v := vs
        */
        void add(lpvar v, unsigned sz, lpvar const* vs);
        void add(lpvar v, svector<lpvar> const& vs) { add(v, vs.size(), vs.c_ptr()); }
        void add(lpvar v, lpvar x, lpvar y) { lpvar vs[2] = { x, y }; add(v, 2, vs); }
        void add(lpvar v, lpvar x, lpvar y, lpvar z) { lpvar vs[3] = { x, y, z }; add(v, 3, vs); }

        /**
           \brief retrieve monomial corresponding to variable v from definition v := vs
        */
        monomial const& var2monomial(lpvar v) const { SASSERT(is_monomial_var(v)); return m_monomials[m_var2index[v]]; }

        monomial const& operator[](lpvar v) const { return var2monomial(v); }

        bool is_monomial_var(lpvar v) const { return m_var2index.get(v, UINT_MAX) != UINT_MAX; }

        /**
           \brief retrieve canonized monomial corresponding to variable v from definition v := vs
        */
        signed_vars const& var2canonical(lpvar v) const { return canonize(var2monomial(v)); }
        
        class canonical {
            emonomials& m;
        public:
            canonical(emonomials& m): m(m) {}
            signed_vars const& operator[](lpvar v) const { return m.var2canonical(v); }
            signed_vars const& operator[](monomial const& mon) const { return m.var2canonical(mon.var()); }
        };

        canonical canonical;
        
        /**
           \brief obtain a canonized signed monomial
           corresponding to current equivalence class.
        */
        signed_vars const& canonize(monomial const& m) const { return m_canonized[m_var2index[m.var()]]; }

        /**
           \brief obtain the representative canonized monomial up to sign.
        */
        signed_vars const& rep(signed_vars const& sv) const { return m_canonized[m_var2index[m_cg_table[sv.var()]]]; }

        /**
           \brief the original sign is defined as a sign of the equivalence class representative.
        */
        rational orig_sign(signed_vars const& sv) const { return rep(sv).rsign(); }           

        /**
           \brief determine if m1 divides m2 over the canonization obtained from merged variables.
         */
        bool canonize_divides(monomial const& m1, monomial const& m2) const;

        /**
           \brief produce explanation for monomial canonization.
        */
        void explain_canonized(monomial const& m, lp::explanation& exp);

        /**
           \brief iterator over monomials that are declared.
        */
        vector<monomial>::const_iterator begin() const { return m_monomials.begin(); }
        vector<monomial>::const_iterator end() const { return m_monomials.end(); }

        /**
           \brief iterators over monomials where an equivalent variable is used
        */
        class iterator {
            emonomials const& m;
            cell*       m_cell;
            bool        m_touched;            
        public:
            iterator(emonomials const& m, cell* c, bool at_end): m(m), m_cell(c), m_touched(at_end || c == nullptr) {}
            monomial const& operator*() { return m.m_monomials[m_cell->m_index]; }
            iterator& operator++() { m_touched = true; m_cell = m_cell->m_next; return *this; }
            iterator operator++(int) { iterator tmp = *this; ++*this; return tmp; }
            bool operator==(iterator const& other) const { return m_cell == other.m_cell && m_touched == other.m_touched; }
            bool operator!=(iterator const& other) const { return m_cell != other.m_cell || m_touched != other.m_touched; }
        };
        
        class use_list {
            emonomials const& m;
            lpvar     m_var;
            cell*     head() { return m.head(m_var); } 
        public:
            use_list(emonomials const& m, lpvar v): m(m), m_var(v) {}
            iterator begin() { return iterator(m, head(), false); }
            iterator end() { return iterator(m, head(), true); }
        };

        use_list get_use_list(lpvar v) const { return use_list(*this, v); }

        /**
           \brief retrieve monomials m' where m is a proper factor of modulo current equalities.
        */
        class pf_iterator {
            emonomials const& m;
            monomial const&   m_mon; // canonized monomial
            iterator          m_it;  // iterator over the first variable occurs list, ++ filters out elements that are not factors.
            iterator          m_end;

            void fast_forward();
        public:
            pf_iterator(emonomials const& m, monomial const& mon, bool at_end);
            monomial const& operator*() { return *m_it; }
            pf_iterator& operator++() { ++m_it; fast_forward(); return *this; }
            pf_iterator operator++(int) { pf_iterator tmp = *this; ++*this; return tmp; }
            bool operator==(pf_iterator const& other) const { return m_it == other.m_it; }
            bool operator!=(pf_iterator const& other) const { return m_it != other.m_it; }
        };

        class factors_of {
            emonomials const& m;
            monomial const& mon;
        public:
            factors_of(emonomials const& m, monomial const& mon): m(m), mon(mon) {}
            pf_iterator begin() { return pf_iterator(m, mon, false); }
            pf_iterator end() { return pf_iterator(m, mon, true); }
        };

        factors_of get_factors_of(monomial const& m) const { inc_visited(); return factors_of(*this, m); }
        factors_of get_factors_of(lpvar v) const { return get_factors_of(var2monomial(v)); }
       
        signed_vars const* find_canonical(svector<lpvar> const& vars) const;

        /**
           \brief iterator over sign equivalent monomials.
           These are monomials that are equivalent modulo m_var_eqs amd modulo signs.
        */
        class sign_equiv_monomials_it {
            emonomials const& m;
            unsigned m_index;
            bool     m_touched;
        public:
            sign_equiv_monomials_it(emonomials const& m, unsigned idx, bool at_end): m(m), m_index(idx), m_touched(at_end) {}
            monomial const& operator*() { return m.m_monomials[m_index]; }
            sign_equiv_monomials_it& operator++() { 
                m_touched = true; 
                m_index = m.m_canonized[m_index].m_next; 
                return *this; 
            }
            sign_equiv_monomials_it operator++(int) { sign_equiv_monomials_it tmp = *this; ++*this; return tmp; }
            bool operator==(sign_equiv_monomials_it const& other) const { 
                return m_index == other.m_index && m_touched == other.m_touched; 
            }
            bool operator!=(sign_equiv_monomials_it const& other) const { 
                return m_index != other.m_index || m_touched != other.m_touched; 
            }
        };

        class sign_equiv_monomials {
            emonomials& em;
            monomial const& m;
            unsigned index() const { return em.m_var2index[m.var()]; }
        public:
            sign_equiv_monomials(emonomials & em, monomial const& m): em(em), m(m) {}
            sign_equiv_monomials_it begin() { return sign_equiv_monomials_it(em, index(), false); }
            sign_equiv_monomials_it end() { return sign_equiv_monomials_it(em, index(), true); }
        };

        sign_equiv_monomials enum_sign_equiv_monomials(monomial const& m) { return sign_equiv_monomials(*this, m); }
        sign_equiv_monomials enum_sign_equiv_monomials(lpvar v) { return enum_sign_equiv_monomials((*this)[v]); }
        sign_equiv_monomials enum_sign_equiv_monomials(signed_vars const& sv) { return enum_sign_equiv_monomials(sv.var()); }

        /**
           \brief display state of emonomials
        */
        std::ostream& display(std::ostream& out) const;

        /**
           \brief
           these are merge event handlers to interect the union-find handlers.
           r2 becomes the new root. r2 is the root of v2, r1 is the old root of v1
        */
        void merge_eh(signed_var r2, signed_var r1, signed_var v2, signed_var v1) override;

        void after_merge_eh(signed_var r2, signed_var r1, signed_var v2, signed_var v1) override;

        void unmerge_eh(signed_var r2, signed_var r1) override;        

    };

    inline std::ostream& operator<<(std::ostream& out, emonomials const& m) { return m.display(out); }

}