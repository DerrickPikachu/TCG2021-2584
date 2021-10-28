/**
 * Framework for 2048 & 2048-like Games (C++ 11)
 * agent.h: Define the behavior of variants of agents including players and environments
 *
 * Author: Theory of Computer Games (TCG 2021)
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include "board.h"
#include "action.h"
#include <fstream>
#include <vector>

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

/**
 * base agent for agents with randomness
 */
class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * random environment
 * add a new random tile to an empty cell
 * 2-tile: 90%
 * 4-tile: 10%
 */
class rndenv : public random_agent {
public:
	rndenv(const std::string& args = "") : random_agent("name=random role=environment " + args),
		space({ 0, 1, 2, 3, 4, 5, 6,
                7, 8, 9, 10, 11, 12, 13,
                14, 15 }), popup(0, 9) {}

	virtual action take_action(const board& after) {
		std::shuffle(space.begin(), space.end(), engine);
		for (int pos : space) {
			if (after(pos) != 0) continue;
			board::cell tile = popup(engine) ? 1 : 2;
			return action::place(pos, tile);
		}
		return action();
	}

private:
	std::array<int, 16> space;
	std::uniform_int_distribution<int> popup;
};

/**
 * dummy player
 * select a legal action randomly
 */
class player : public random_agent {
public:
	player(const std::string& args = "") : random_agent("name=dummy role=player " + args),
		opcode({ 0, 1, 2, 3 }),
        tuples({{0, 1, 2, 3}}),
        play_type(args) {}

	virtual action take_action(const board& before) {
		if (play_type == "greedy") {
            return greedy_action(before);
        } else if (play_type == "heuristic") {
            return heuristic_action(before);
        } else {
            return random_action(before);
        }
	}

    action random_action(const board& before) {
        std::shuffle(opcode.begin(), opcode.end(), engine);
        for (int op : opcode) {
            board::reward reward = board(before).slide(op);
            if (reward != -1) return action::slide(op);
        }
        return action();
    }

    action greedy_action(const board& before) {
        int max_reward = 0, best_op = -1;
        for (int op : opcode) {
            board::reward reward = board(before).slide(op);
            if (reward != -1 && max_reward <= reward) {
                max_reward = reward;
                best_op = op;
            }
        }
        return (best_op != -1)? action::slide(best_op) : action();
    }

    action heuristic_action(const board& before) {
        int max_score = 0, best_op = -1;
        for (int op : opcode) {
            board after = before;
            board::reward reward = after.slide(op);
            if (reward != -1) {
                int critic = tree_search(after);
                if (max_score <= reward + critic) {
                    max_score = reward + critic;
                    best_op = op;
                }
            }
        }
        return (best_op != -1)? action::slide(best_op) : action();
    }

private:
    /**
     * Those methods are used in heuristic action selector
     * Not for public
     */
    int tree_search(board& game, int search_depth=1) {
        if (search_depth <= 0)
            return evaluate_board(game);
        int best_score = 0;
        for (int op : opcode) {
            board after = game;
            board::reward reward = after.slide(op);
            int score = 0;
            if (reward != -1)
                score = reward + tree_search(after, search_depth - 1);
            best_score = std::max(best_score, score + reward);
        }
        return best_score;
    }

    int evaluate_board(board& after) {
        int score = 0;
        for (std::array<int, 4> tuple : tuples) {
            for (int i = 0; i < 4; i++) {
                score += cal_decreasing_score(tuple, after);
                after.rotate_left();
            }
        }
        score += cal_space_score(after);
        return score;
    }

    int cal_maxtile_score(board& after) {
        int max_tile = 0, pos = -1;
        for (int i = 0; i < 16; i++) {
            if (after(i) > (uint32_t)max_tile) {
                max_tile = after(i);
                pos = i;
            }
        }
        if (pos == 0 || pos == 3 || pos == 12 || pos == 15)
            return max_tile;
        return 0;
    }

    int cal_decreasing_score(std::array<int, 4>& tuple, board& after) {
        bool is_decreasing = true, is_increasing = true;
        int score = 0;
        for (int i = 1; i < (int)tuple.size(); i++) {
            score += board::map_to_fibonacci(after(tuple[i]));
            if (after(tuple[i]) == after(tuple[i-1]))
                return 0;
            else if (after(tuple[i]) > after(tuple[i-1]))
                is_decreasing = false;
            else
                is_increasing = false;
        }
        return (is_decreasing || is_increasing)? score : 0;
    }

    int cal_space_score(board& after) {
        int space_counter = 0, space_factor = 5;
        for (int i = 0; i < 16; i++) {
            space_counter += (after(i) == 0)? 1 : 0;
        }
        return space_counter * space_factor;
    }

private:
	std::array<int, 4> opcode;
    std::vector<std::array<int, 4>> tuples;
    std::string play_type;
};
