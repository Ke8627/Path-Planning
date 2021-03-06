#pragma once

#include "helpers.hpp"
#include "priority_queue.hpp"
#include "timing.hpp"

using std::pair;
using std::make_pair;
using std::min_element;

namespace search
{
    namespace ds
    {
        struct OldKeys : public unordered_map<Cell, Key, Cell::Hasher>
        {
            auto update_with(pair<Cell, Key> cell_key_pair)
            {
                erase(cell_key_pair.first);
                insert(cell_key_pair);
            }
        };

        class DStarCore
        {
            //
            //  Algorithm
            //

            auto validate(Cell c) const
            {
                return c.row >= 0 && c.row < (int)matrix.rows() && c.col >= 0 && c.col < (int)matrix.cols();
            }
            auto valid_neighbours_of(Cell c) const
            {
                Cells neighbours;
                for (auto direction = '1'; direction != '9'; ++direction)
                {
                    auto n = DIRECTIONS.at(direction)(c);
                    if (validate(n))
                        neighbours.insert(n);
                }
                return neighbours;
            }
            auto build_path(Cell beg, Cell end) 
            {
                string path;
                for (auto cur = beg; cur != end; )
                {
                    for (auto direction = '1'; direction != '9'; ++direction)
                    {
                        auto n = DIRECTIONS.at(direction)(cur);
                        if (validate(n) && !at(n).bad && (at(n).g + cost() == at(cur).g))
                        {
                            path += direction;
                            cur = n;
                            break;
                        }
                    }
                }
                return path;
            }
            auto initialize()
            {
                q.reset();
                km = at(goal).r = 0;
                q.push(goal);
                old_keys.insert({ goal, { at(goal), km } });
            }
            auto update_vertex(LpState& s)
            {
                if (s.cell != goal)
                {
                    auto minimum = huge();
                    for (auto neighbour : valid_neighbours_of(s.cell))
                        minimum = min(minimum, (at(neighbour).g + cost()));
                    s.r = minimum;
                }
                q.remove(s.cell);
                if (s.g != s.r)
                    q.push(s.cell), 
                    old_keys.update_with( { s.cell, Key{ s, km } } );
            }
            auto update_neighbours_of(Cell cell)
            {
                for (auto neighbour : valid_neighbours_of(cell))
                    if (!at(neighbour).bad)
                        update_vertex(at(neighbour));
            }
            auto compute_shortest_path()
            {
                for (Timing t{ run_time }; !q.empty() && (Key{ at(q.top()) } < Key{ at(start) } || at(start).r != at(start).g);)
                {
                    auto c = q.pop();

                    if (old_keys.at(c) < Key{ at(c), km })
                        q.push(c), 
                        old_keys.update_with({ c, Key{ at(c), km } });
                    else if (at(c).g > at(c).r)
                        at(c).g = at(c).r,
                        update_neighbours_of(c);
                    else
                        at(c).g = huge(), 
                        update_vertex(at(c)),
                        update_neighbours_of(c);

                    {
                        max_q_size = max(max_q_size, q.size());
                        expansions.insert(c);
                    }
                }
            }
            
            //
            //  helpers
            //

            auto at(Cell c) const -> LpState const&
            {
                return matrix.at(c);
            }
            auto at(Cell c) -> LpState&
            {
                return matrix.at(c);
            }
            auto mark_bad_cells(Cells const& bad_cells)
            {
                for (auto c : bad_cells) at(c).bad = true;
            }
            auto mark_h_values_with(Cell terminal)
            {
                auto mark_h = [=](Cell c) { at(c).h = hfunc(c, terminal); };
                matrix.each_cell(mark_h);
            }
            auto reset_statistics()
            {
                run_time = max_q_size = 0;
                expansions.clear();
            }

        public:
            //
            //  Constructor
            //
            DStarCore(unsigned rows, unsigned cols, Cell start, Cell goal, string heuristic, Cells const& bad_cells) :
                matrix{ rows, cols },
                start{ start },
                goal{ goal },
                hfunc{ HEURISTICS.at(heuristic) },
                km{ 0 },
                q{ [this](Cell l, Cell r) { return Key{ at(l), km } < Key{ at(r), km }; } },
                old_keys { }
            {
                mark_bad_cells(bad_cells);
                mark_h_values_with(start);  //h value : start to current
                reset_statistics();
            }

            auto initial_plan()
            {
                initialize();
                compute_shortest_path();
                return build_path(start, goal);
            }

            //  MoveTo :        callback with argument (Cell curr)
            //  OnPathBuilt :   callback with argument (string path)
            template<typename MoveTo, typename OnPathBuilt>
            auto plan(vector<Cells> && changes, MoveTo && move_to, OnPathBuilt && use_path)
            {
                initial_plan();
                
                struct Variables 
                { 
                    Cell last, curr; 
                    decltype(changes.cbegin()) changes_iterator; 
                };

                for (Variables this_loop = { start, start, changes.cbegin() }; this_loop.curr != goal;)
                {
                    auto less = [this](Cell l, Cell r) { return at(l).g + cost() < at(r).g + cost(); };
                    auto ns = valid_neighbours_of(this_loop.curr);
                    this_loop.curr = *min_element(ns.cbegin(), ns.cend(), less);

                    move_to(this_loop.curr);
                    if (this_loop.changes_iterator != changes.cend())
                    {
                        km += hfunc(this_loop.last, this_loop.curr);
                        this_loop.last = this_loop.curr;
                        for (auto cell : *this_loop.changes_iterator)
                        {
                            at(cell).bad = !at(cell).bad;
                            if (!at(cell).bad)
                                update_vertex(at(cell));
                            else
                                at(cell).g = at(cell).r = huge();
                            update_neighbours_of(cell);
                        } 
                        ++this_loop.changes_iterator;
                        compute_shortest_path();
                    }
                    use_path(build_path(this_loop.curr, goal));
                }
            }

            //
            //  data members
            //

            Matrix matrix;
            Cell const start, goal;
            function<int(Cell, Cell)> const hfunc;
            int km;
            PriorityQueue < Cell, function<bool(Cell, Cell)> > q;
            OldKeys old_keys;
            
            //
            //  statistics
            //
            
            size_t max_q_size;
            Cells expansions;
            long long run_time;
        };
    }
}
