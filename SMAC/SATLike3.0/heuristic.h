#ifndef _HEURISTIC_H_
#define _HEURISTIC_H_

#include "basis_pms.h"
#include "deci.h"

void Satlike::init(vector<int> &init_solution)
{
    soft_large_weight_clauses_count = 0;
    //Initialize clause information
    for (int c = 0; c < num_clauses; c++)
    {
        already_in_soft_large_weight_stack[c] = 0;
        clause_selected_count[c] = 0;

        if (org_clause_weight[c] == top_clause_weight)
            clause_weight[c] = 1;
        else
        {
            if (best_soln_feasible == 0)
            {
                clause_weight[c] = 0;
            }
            else
            {
                clause_weight[c] = org_clause_weight[c];
                if (clause_weight[c] > 1 && already_in_soft_large_weight_stack[c] == 0)
                {
                    already_in_soft_large_weight_stack[c] = 1;
                    soft_large_weight_clauses[soft_large_weight_clauses_count++] = c;
                }
            }
        }
    }

    //init solution
    if (best_soln_feasible == 1)
    {
        best_soln_feasible = 2;
        for (int v = 1; v <= num_vars; v++)
        {
            //cur_soln[v] = rand() % 2;
            time_stamp[v] = 0;
            unsat_app_count[v] = 0;
        }
    }
    else if (init_solution.size() == 0)
    {
        for (int v = 1; v <= num_vars; v++)
        {
            cur_soln[v] = rand() % 2;
            time_stamp[v] = 0;
            unsat_app_count[v] = 0;
        }
    }
    else
    {
        for (int v = 1; v <= num_vars; v++)
        {
            cur_soln[v] = init_solution[v];
            if (cur_soln[v] != 0 && cur_soln[v] != 1)
                cur_soln[v] = rand() % 2;
            time_stamp[v] = 0;
            unsat_app_count[v] = 0;
        }
    }

    //init stacks
    hard_unsat_nb = 0;
    soft_unsat_weight = 0;
    hardunsat_stack_fill_pointer = 0;
    softunsat_stack_fill_pointer = 0;
    unsatvar_stack_fill_pointer = 0;
    large_weight_clauses_count = 0;

    /* figure out sat_count, sat_var and init unsat_stack */
    for (int c = 0; c < num_clauses; ++c)
    {
        sat_count[c] = 0;
        for (int j = 0; j < clause_lit_count[c]; ++j)
        {
            if (cur_soln[clause_lit[c][j].var_num] == clause_lit[c][j].sense)
            {
                sat_count[c]++;
                sat_var[c] = clause_lit[c][j].var_num;
            }
        }
        if (sat_count[c] == 0)
        {
            unsat(c);
        }
    }

    /*figure out score*/
    for (int v = 1; v <= num_vars; v++)
    {
        score[v] = 0;
        for (int i = 0; i < var_lit_count[v]; ++i)
        {
            int c = var_lit[v][i].clause_num;
            if (sat_count[c] == 0)
                score[v] += clause_weight[c];
            else if (sat_count[c] == 1 && var_lit[v][i].sense == cur_soln[v])
                score[v] -= clause_weight[c];
        }
    }

    //init goodvars stack
    goodvar_stack_fill_pointer = 0;
    for (int v = 1; v <= num_vars; v++)
    {
        if (score[v] > 0)
        {
            already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
            mypush(v, goodvar_stack);
        }
        else
            already_in_goodvar_stack[v] = -1;
    }

    //cout << goodvar_stack_fill_pointer << endl;
    //cout << hard_unsat_nb << endl;
    //cout << soft_unsat_weight << endl;
#ifdef IOH_OUTPUT
    // recording.
    ioh_recording();
#endif
}

int Satlike::pick_var()
{
    int i, v;
    int best_var;

    if (goodvar_stack_fill_pointer > 0)
    {
        if ((rand() % MY_RAND_MAX_INT) * BASIC_SCALE < rdprob)
            return goodvar_stack[rand() % goodvar_stack_fill_pointer];

        if (goodvar_stack_fill_pointer < hd_count_threshold)
        {
            best_var = goodvar_stack[0];
            for (i = 1; i < goodvar_stack_fill_pointer; ++i)
            {
                v = goodvar_stack[i];
                if (score[v] > score[best_var])
                    best_var = v;
                else if (score[v] == score[best_var])
                {
                    if (time_stamp[v] < time_stamp[best_var])
                        best_var = v;
                }
            }
            return best_var;
        }
        else
        {
            best_var = goodvar_stack[rand() % goodvar_stack_fill_pointer];
            for (i = 1; i < hd_count_threshold; ++i)
            {
                v = goodvar_stack[rand() % goodvar_stack_fill_pointer];
                if (score[v] > score[best_var])
                    best_var = v;
                else if (score[v] == score[best_var])
                {
                    if (time_stamp[v] < time_stamp[best_var])
                        best_var = v;
                }
            }
            return best_var;
        }
    }

    update_clause_weights();

    int sel_c;
    lit *p;

    if (hardunsat_stack_fill_pointer > 0)
    {
        sel_c = hardunsat_stack[rand() % hardunsat_stack_fill_pointer];
    }
    else
    {
        sel_c = softunsat_stack[rand() % softunsat_stack_fill_pointer];
    }
    if ((rand() % MY_RAND_MAX_INT) * BASIC_SCALE < rwprob)
        return clause_lit[sel_c][rand() % clause_lit_count[sel_c]].var_num;

    best_var = clause_lit[sel_c][0].var_num;
    p = clause_lit[sel_c];
    for (p++; (v = p->var_num) != 0; p++)
    {
        if (score[v] > score[best_var])
            best_var = v;
        else if (score[v] == score[best_var])
        {
            if (time_stamp[v] < time_stamp[best_var])
                best_var = v;
        }
    }

    return best_var;
}

void Satlike::local_search(char *inputfile)
{
    vector<int> init_solution;
    settings();
    for (tries = 1; tries < max_tries; ++tries)
    {
        init(init_solution);

        for (step = 1; step < max_flips; ++step)
        {
            if (hard_unsat_nb == 0 && (soft_unsat_weight < opt_unsat_weight || best_soln_feasible == 0))
            {
                if (soft_unsat_weight < opt_unsat_weight)
                {
                    best_soln_feasible = 1;
                    opt_unsat_weight = soft_unsat_weight;
                    opt_time = get_runtime();
                    for (int v = 1; v <= num_vars; ++v)
                        best_soln[v] = cur_soln[v];
                }
                if (opt_unsat_weight == 0)
                    return;
            }

            if (step % 1000 == 0)
            {
                double elapse_time = get_runtime();
                if (elapse_time >= cutoff_time)
                    return;
                else if (opt_unsat_weight == 0)
                    return;
            }

            int flipvar = pick_var();
            flip(flipvar);
            time_stamp[flipvar] = step;
        }
    }
}

void Satlike::local_search_with_decimation(char *inputfile)
{
    // settings();
#ifdef IOH_OUTPUT    
    // recording.
    ioh_recording_init();
#endif

    Decimation deci(var_lit, var_lit_count, clause_lit, org_clause_weight, top_clause_weight);
    deci.make_space(num_clauses, num_vars);

    opt_unsat_weight = __LONG_LONG_MAX__;
    for (tries = 1; tries < max_tries; ++tries)
    {
        if (best_soln_feasible != 1)
        {
            deci.init(local_opt_soln, best_soln, unit_clause, unit_clause_count, clause_lit_count);
            deci.unit_prosess();
            init(deci.fix);
        }
        else
            init(deci.fix);

        long long local_opt = __LONG_LONG_MAX__;
        max_flips = max_non_improve_flip;
        for (step = 1; step < max_flips; ++step)
        {
            if (hard_unsat_nb == 0)
            {
                if (local_opt > soft_unsat_weight)
                {
                    local_opt = soft_unsat_weight;
                    max_flips = step + max_non_improve_flip;
                }
                if (soft_unsat_weight < opt_unsat_weight)
                {
                    // cout << "o " << soft_unsat_weight << " " << get_runtime() << endl;
                    cout << soft_unsat_weight << " " << get_runtime() << endl;
                    opt_unsat_weight = soft_unsat_weight;
                    opt_time = get_runtime();
                    for (int v = 1; v <= num_vars; ++v)
                        best_soln[v] = cur_soln[v];
                }
                if (best_soln_feasible == 0)
                {
                    best_soln_feasible = 1;
                    // break;
                }
            }
            //if(goodvar_stack_fill_pointer==0) cout<<step<<": 0"<<endl;
         
            if (step % 1000 == 0)
            {
                double elapse_time = get_runtime();
                if (elapse_time >= cutoff_time)
                {
                    // if (best_soln_feasible == 1) {
                    //     cout << opt_unsat_weight << endl;
                    // }
                    // else {
                    //     cout << __LONG_LONG_MAX__ << endl;
                    // }
                    return;
                }
                else if (opt_unsat_weight == 0)
                {
                    // cout << opt_unsat_weight << endl;
                    return;
                }
            }

            int flipvar = pick_var();
            flip(flipvar);
            time_stamp[flipvar] = step;
        }
    }
}

void Satlike::increase_weights()
{
    int i, c, v;
    for (i = 0; i < hardunsat_stack_fill_pointer; ++i)
    {
        c = hardunsat_stack[i];
        clause_weight[c] += h_inc;

        if (clause_weight[c] == (h_inc + 1))
            large_weight_clauses[large_weight_clauses_count++] = c;

        for (lit *p = clause_lit[c]; (v = p->var_num) != 0; p++)
        {
            score[v] += h_inc;
            if (score[v] > 0 && already_in_goodvar_stack[v] == -1)
            {
                already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
                mypush(v, goodvar_stack);
            }
        }
    }
    for (i = 0; i < softunsat_stack_fill_pointer; ++i)
    {
        c = softunsat_stack[i];
        if (clause_weight[c] > softclause_weight_threshold)
            continue;
        else
            clause_weight[c]++;

        if (clause_weight[c] > 1 && already_in_soft_large_weight_stack[c] == 0)
        {
            already_in_soft_large_weight_stack[c] = 1;
            soft_large_weight_clauses[soft_large_weight_clauses_count++] = c;
        }
        for (lit *p = clause_lit[c]; (v = p->var_num) != 0; p++)
        {
            score[v]++;
            if (score[v] > 0 && already_in_goodvar_stack[v] == -1)
            {
                already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
                mypush(v, goodvar_stack);
            }
        }
    }
}

void Satlike::smooth_weights()
{
    int i, clause, v;

    for (i = 0; i < large_weight_clauses_count; i++)
    {
        clause = large_weight_clauses[i];
        if (sat_count[clause] > 0)
        {
            clause_weight[clause] -= h_inc;

            if (clause_weight[clause] == 1)
            {
                large_weight_clauses[i] = large_weight_clauses[--large_weight_clauses_count];
                i--;
            }
            if (sat_count[clause] == 1)
            {
                v = sat_var[clause];
                score[v] += h_inc;
                if (score[v] > 0 && already_in_goodvar_stack[v] == -1)
                {
                    already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
                    mypush(v, goodvar_stack);
                }
            }
        }
    }

    for (i = 0; i < soft_large_weight_clauses_count; i++)
    {
        clause = soft_large_weight_clauses[i];
        if (sat_count[clause] > 0)
        {
            clause_weight[clause]--;
            if (clause_weight[clause] == 1 && already_in_soft_large_weight_stack[clause] == 1)
            {
                already_in_soft_large_weight_stack[clause] = 0;
                soft_large_weight_clauses[i] = soft_large_weight_clauses[--soft_large_weight_clauses_count];
                i--;
            }
            if (sat_count[clause] == 1)
            {
                v = sat_var[clause];
                score[v]++;
                if (score[v] > 0 && already_in_goodvar_stack[v] == -1)
                {
                    already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
                    mypush(v, goodvar_stack);
                }
            }
        }
    }
}

void Satlike::update_clause_weights()
{
    if (((rand() % MY_RAND_MAX_INT) * BASIC_SCALE) < smooth_probability && large_weight_clauses_count > large_clause_count_threshold)
    {
        smooth_weights();
    }
    else
    {
        increase_weights();
    }
}

#endif
