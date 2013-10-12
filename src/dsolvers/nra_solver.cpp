/*********************************************************************
Author: Soonho Kong <soonhok@cs.cmu.edu>
        Sicun Gao <sicung@cs.cmu.edu>
        Edmund Clarke <emc@cs.cmu.edu>

dReal -- Copyright (C) 2013, Soonho Kong, Sicun Gao, and Edmund Clarke

dReal is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

dReal is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with dReal. If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "dsolvers/nra_solver.h"
#include "dsolvers/icp_solver.h"
#include <utility>
#include <boost/algorithm/string/predicate.hpp>

using std::pair;
using boost::starts_with;

NRASolver::NRASolver(const int i, const char * n, SMTConfig & c, Egraph & e, SStore & t,
                     vector< Enode * > & x, vector<Enode *> & d, vector<Enode *> & s)
    : OrdinaryTSolver (i, n, c, e, t, x, d, s) {
    // initialize icp solver first
    if (c.nra_precision == 0.0)
        c.nra_precision = 0.001;
}

NRASolver::~NRASolver() {
    // Here Deallocate External Solver
}

void debug_print_env(const scoped_map<Enode*, pair<double, double>> & env) {
    for (auto ite = env.begin(); ite != env.end(); ite++) {
        Enode* key = ite->first;
        const double lb  = ite->second.first;
        const double ub  = ite->second.second;
        cerr << "Key: " << key
             << "\t Value: [" << lb << ", " << ub << "]"
             << endl;
    }
}

void debug_print_stack(const vector<Enode*> & stack) {
    // Print out all the Enode in stack
    for (auto ite = stack.begin(); ite != stack.end(); ite++) {
        cerr << "asserted literal : " << *ite
             << "\t" << (*ite)->hasPolarity()
             << endl;
    }
}

void debug_print_explanation (const vector<Enode*> & explanation) {
    for (auto ite = explanation.begin(); ite!= explanation.end(); ite++) {
        cerr << *ite <<" with polarity "
             << toInt((*ite)->getPolarity()) << " ";
    }
    cerr << endl;
}

// Collect all the variables appeared in e
set<Enode *> NRASolver::get_variables (Enode * e) {
    set<Enode *> result;
    Enode * p = nullptr;
    if ( e->isSymb()) { /* do nothing */ }
    else if (e->isNumb()) { /* do nothing */ }
    else if (e->isTerm()) {
        if (e -> isVar()) { result.insert(e); }
        set<Enode*> tmp_set = get_variables(e->getCar());
        result.insert(tmp_set.begin(), tmp_set.end());
        p = e->getCdr();
        while (!p->isEnil()) {
            tmp_set = get_variables(p->getCar());
            result.insert(tmp_set.begin(), tmp_set.end());
            p = p->getCdr();
        }
    } else if (e->isList()) {
        if (!e->isEnil()) {
            set <Enode*> tmp_set = get_variables(e->getCar());
            result.insert(tmp_set.begin(), tmp_set.end());
            p = e->getCdr();
            while (!p->isEnil()) {
                tmp_set = get_variables(p->getCar());
                result.insert(tmp_set.begin(), tmp_set.end());
                p = p->getCdr();
            }
        }
    } else if (e->isDef()) { /* do nothing */ }
    else if (e->isEnil()) { /* do nothing */ }
    else opensmt_error("unknown case value");
    return result;
}

// The solver is informed of the existence of atom e. It might be
// useful for initializing the solver's data structures. This function
// is called before the actual solving starts.
//
// `inform` does two operations:
// 1. set up environment (mapping from enode to its [lb, ub])
// 2. set up _enode_to_vars (mapping from an expr to all the
//    variables in it)
lbool NRASolver::inform(Enode * e) {
    if (config.nra_verbose) {
        cerr << "================================================================" << endl
             << "NRASolver::inform: " << e
             << " with polarity " << e->getPolarity().toInt() << endl
             << "================================================================" << endl;
    }
    assert(e->isAtom());
    set<Enode*> variables_in_e = get_variables(e);
    set<Enode*> ode_variables_in_e;
    for (auto ite = variables_in_e.begin(); ite != variables_in_e.end(); ite++) {
        if (config.nra_verbose) {
            cerr << *ite << endl;
        }
        double lb = (*ite)->getLowerBound();
        double ub = (*ite)->getUpperBound();
        env.insert(*ite, make_pair(lb, ub));

        // Collect ODE Vars in e
        if (config.nra_contain_ODE && (*ite)->getODEtimevar() != nullptr && (*ite)->getODEgroup() > 0) {
            if (config.nra_verbose) {
                cerr << "Add " << *ite << " in the bag!!!! " << endl;
                cerr << "\t Group: " << (*ite)->getODEgroup() << endl;
            }
            ode_variables_in_e.insert(*ite);
        }
    }
    if (config.nra_contain_ODE) {
        _enode_to_vars.insert(pair<Enode*, set<Enode*> >(e, ode_variables_in_e));
    }
    return l_Undef;
}

// Asserts a literal into the solver. If by chance you are able to
// discover inconsistency you may return false. The real consistency
// state will be checked with "check"
//
// assertLit adds a literal(e) to stack which represetns a list of
// asserted literals.
bool NRASolver::assertLit (Enode * e, bool reason) {
    if (config.nra_verbose) {
        cerr << "================================================================" << endl
             << "NRASolver::assertLit: " << e
             << ", reason: " << (reason ? "true" : "false")
             << ", polarity: " << e->getPolarity().toInt() << endl
             << "================================================================" << endl;
    }
    (void)reason;
    assert(e);
    assert(belongsToT(e));
    assert(e->hasPolarity());
    assert(e->getPolarity() == l_False || e->getPolarity() == l_True);
    if (e->isDeduced() && e->getPolarity() == e->getDeduced() && e->getDedIndex() == id) {
        if (config.nra_verbose) {
            cerr << "NRASolver::assertLit: DEDUCED" << e << endl;
        }
        return true;
    }
    stack.push_back(e);
    return true;
}

// Saves a backtrack point You are supposed to keep track of the
// operations, for instance in a vector called "undo_stack_term", as
// happens in EgraphSolver
void NRASolver::pushBacktrackPoint () {
    if (config.nra_verbose) {
        cerr << "================================================================" << endl
             << "NRASolver::pushBacktrackPoint " << stack.size() << endl;
    }
    env.push();
    undo_stack_size.push_back(stack.size());
    if (config.nra_verbose) {
        cerr << "================================================================" << endl;
    }
}

// Restore a previous state. You can now retrieve the size of the
// stack when you pushed the last backtrack point. You have to
// implement the necessary backtrack operations (see for instance
// backtrackToStackSize() in EgraphSolver) Also make sure you clean
// the deductions you did not communicate
void NRASolver::popBacktrackPoint () {
    if (config.nra_verbose) {
        cerr << "================================================================" << endl
             << "NRASolver::popBacktrackPoint" << endl;
    }
    unsigned prev_size = undo_stack_size.back();
    undo_stack_size.pop_back();
    unsigned cur_size = stack.size();

    while (cur_size-- > prev_size) {
        stack.pop_back();
    }
    env.pop();
}

//
// Check for consistency. If flag is
// set make sure you run a complete check
//
bool NRASolver::check(bool complete) {
    if (config.nra_verbose) {
        cerr << "================================================================" << endl;
        cerr << "NRASolver::check " << (complete ? "complete" : "incomplete") << endl;
        for (auto ite = env.begin(); ite != env.end(); ite++) {
            Enode* key = (*ite).first;
            double lb =  (*ite).second.first;
            double ub =  (*ite).second.second;
            if (starts_with(key->getCar()->getName(), "mode_")) {
                cerr << "Key: " << key << "\t Value: [" << lb << ", " << ub << "]" << endl;
            }
        }
        cerr << "================================================================" << endl;
    }
    bool result = true;
    if (config.nra_verbose) {
        debug_print_stack(stack);
        debug_print_env(env);
    }
//    env = env_stack.back();
    icp_solver solver(config, stack, env, explanation, _enode_to_vars);
    if (!complete) {
        // Incomplete Check
        if (config.nra_verbose) {
            cerr << "Incomplete Check" << endl;
            cerr << "Before Prop" << endl;
            debug_print_env(env);
        }
        result = solver.prop();
        if (config.nra_verbose) {
            cerr << "After Prop" << endl;
            debug_print_env(env);
        }
//        env_stack.pop_back();
//        env_stack.push_back(env);
    } else {
        // Complete Check
        // if (config.nra_json) {
        //     config.nra_json_out << "{" << endl
        //                         << "\"traces\": ";
        // }
        result = solver.solve();
    }
    // If the result is UNSAT, generate explanation
    if (!result) {
        if (config.nra_verbose) {
            cerr << "#explanation provided: ";
            debug_print_explanation(explanation);
        }
    }
    // if (!result && config.nra_contain_ODE && config.nra_json) {
    //     // Reset Stream
    //     config.nra_json_out.seekp(ios_base::beg);
    // }
    if (complete && result && config.nra_contain_ODE && config.nra_json) {
        // Print out ODE trajectory
        config.nra_json_out << "{\"traces\": " << endl
                            << "[" << endl
                            << "[]" << endl;
        solver.print_ODE_trajectory();
        config.nra_json_out << "]" << endl;

        // collect all the ODE groups in the asserted literal and
        // print out
        set<int> ode_groups;

        for (auto lit = stack.begin(); lit != stack.end(); lit++) {
            if ((*lit)->getPolarity() == l_True) {
                set<Enode*> variables_in_lit = _enode_to_vars[*lit];
                for (auto var = variables_in_lit.begin(); var != variables_in_lit.end(); var++) {
                    if ((*var)->getODEvartype() == l_True) {
                        ode_groups.insert((*var)->getODEgroup());
                    }
                }
            }
        }

        for (auto ite = env.begin(); ite != env.end(); ite++) {
            Enode* key = (*ite).first;
            double lb =  (*ite).second.first;
            double ub =  (*ite).second.second;
            if (starts_with(key->getCar()->getName(), "mode_")) {
                cerr << "Key: " << key << "\t Value: [" << lb << ", " << ub << "]" << endl;
            }
        }
        config.nra_json_out << ", \"groups\": [";
        for (auto g = ode_groups.begin();
             g != ode_groups.end();
             g++) {
            if (g != ode_groups.begin()) {
                config.nra_json_out << ", ";
            }
            config.nra_json_out << *g;
        }
        config.nra_json_out << "]" << endl
                            << "}" << endl;
    }
    return result;
}

// Return true if the enode belongs to this theory. You should examine
// the structure of the node to see if it matches the theory operators
bool NRASolver::belongsToT(Enode * e) {
    (void)e;
    assert(e);
    return true;
}

// Copy the model into enode's data
void NRASolver::computeModel() {
    if (config.nra_verbose) {
        cerr << "computeModel" << endl;
    }
}

#ifdef PRODUCE_PROOF
Enode * NRASolver::getInterpolants() {
    return nullptr;
}
#endif