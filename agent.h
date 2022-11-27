/**
 * Framework for NoGo and similar games (C++ 11)
 * agent.h: Define the behavior of variants of the player
 *
 * Author: Theory of Computer Games
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
#include <fstream>
#include <time.h>
#include <stdlib.h>
#include "board.h"
#include "action.h"

#define CHILDNODESIZE 81
#define _b 0.025

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
	virtual std::string search() const { return property("search"); }
	virtual std::string sim_time() const { return property("simulation"); }

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
 * random player for both side
 * put a legal piece randomly
 */
class player : public random_agent {
public:
	typedef struct node {
		struct node *child[CHILDNODESIZE];
		int count, val, rave_count, rave_val;
		bool isLeaf;
		board::piece_type color;
	} node_t;

	player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
        srand(time(NULL));
        if (search() == "MCTS")
            simulation_times = stoi(sim_time());
	}

    double beta (int count, int rave_count) {
        return rave_count / (rave_count + count + 4 * (double) rave_count * (double) count * pow((_b), 2));
    }

	virtual action take_action(const board& state) {
		if (search() == "Random") {
			std::shuffle(space.begin(), space.end(), engine);
			for (const action::place& move : space) {
				board after = state;
				if (move.apply(after) == board::legal)
					return move;
			}
			return action();
		} else if (search() == "MCTS") {
            bool flag = false;
			for (const action::place& move : space) {
				board after = state;
				if (move.apply(after) == board::legal)
                    flag = true;
            }
            if (!flag)
                return action();

            std::vector<int> indexs;
            for (int i = 0; i < CHILDNODESIZE; ++i)
                indexs.push_back(i);
            std::shuffle(indexs.begin(), indexs.end(), engine);

			node_t *root = (node_t *) malloc (sizeof(node_t));
			root->isLeaf = true;
			root->count = root->rave_count = root->val = root->rave_val = 0;
			root->color = who;
			for (int i = 0; i < simulation_times; ++i) {
				playOneSequence(root, state, indexs);
                //printf("%d\n", i);
            }

			int index = 0;
            while (root->child[indexs[index]]->count == 0)
                index += 1;
            node_t *p = root->child[indexs[index]];
            double max_q = (double) p->val /  p->count;
            double max_q_rave = (double) p->rave_val / p->rave_count;
            double max_beta_ = beta(p->count, p->rave_count);
            double max = (1 - max_beta_) * max_q + max_beta_ * max_q_rave;
			for (int i = index + 1; i < CHILDNODESIZE; ++i) {
                node_t *cur = root->child[indexs[i]];
                if (cur->count != 0) {
                    double q = (double) cur->val /  cur->count;
                    double q_rave = (double) cur->rave_val /  cur->rave_count;
                    double beta_ = beta(cur->count, cur->rave_count);
                    if ((1 - beta_) * q + beta_ * q_rave > max) {
    				    index = i;
                        max = (1 - beta_) * q + beta_ * q_rave;
                    }
                }
            }

            free_tree(root);

            /*
            board::grid stone = board(state).getStone();
            show_board(stone);
            printf("move index: %d\n", indexs[index]);
            */

            int tmp = indexs[index];
            indexs.clear();

			return action::place(tmp, who);
		}
		return action();
	}

	unsigned my_close_episode(const std::string& flag = "") {
		return flag == "black" ? 1u : 2u;
	}

	node_t *select (node_t *parent, board& presentBoard, board::piece_type color, int move[CHILDNODESIZE + 1], int step, std::vector<int> indexs) {
		int total = 0;
		double v[CHILDNODESIZE] = {0.0};
		for (int i = 0; i < CHILDNODESIZE; ++i)
			total += parent->child[i]->count;

		for (int i = 0; i < CHILDNODESIZE; ++i) {
			board after = presentBoard;
            node_t *child = parent->child[i];
			if (action::place(i, parent->color).apply(after) == board::legal) {
                double q = (double) child->val /  child->count;
                double q_rave = (double) child->rave_val /  child->rave_count;
                double beta_ = beta(child->count, child->rave_count);
				if (parent->color == color)
					v[i] = child->count == 0 ? 1e308 : (1 - beta_) * q + beta_ * q_rave + pow(2 * log10(total) / child->count, 0.5);
				else
					v[i] = child->count == 0 ? 0 : (1 - beta_) * q + beta_ * q_rave + pow(2 * log10(total) / child->count, 0.5);
			} else
				v[i] = parent->color == color ? -1 : 1.2e308;
		}

        std::shuffle(indexs.begin(), indexs.end(), engine);

		int i = 0;
		for (int j = i + 1; j < CHILDNODESIZE; ++j) {
			if (parent->color == color)
				i = v[indexs[j]] > v[indexs[i]] ? j : i;
			else
				i = v[indexs[j]] < v[indexs[i]] ? j : i;
		}
		presentBoard.setBoard(indexs[i], parent->color);

        int tmp = indexs[i];
        move[step] = tmp;

        // change turn
        presentBoard.change_turn();

		return parent->child[tmp];
	}

	inline void updateValue (node_t *selectNode[CHILDNODESIZE], int value, int last, bool isEndBoard, int move[CHILDNODESIZE + 1]) {
		node_t *p = selectNode[last];
        if (!isEndBoard) {
		    p->isLeaf = false;
            board::piece_type child_color = p->color == board::piece_type::black ? board::piece_type::white : board::piece_type::black;
    		for (int i = 0; i < CHILDNODESIZE; ++i) {
	    		p->child[i] = (node_t *) malloc (sizeof(node_t));
                node_t *child = p->child[i];
		    	child->isLeaf = true;
	    		child->color = child_color;
		    	child->count = child->val = child->rave_count = child->rave_val = 0;
    		}   
        }
		for (int i = last; i >= 0; --i) {
            for (int j = i + 2; j < last; j += 2) {
                selectNode[i]->child[move[j]]->rave_val += value;
                selectNode[i]->child[move[j]]->rave_count += 1;
            }
			selectNode[i]->val += value;
			selectNode[i]->count += 1;
            if (!((last - i) & 0x1)) {
			    selectNode[i]->rave_val += value;
    			selectNode[i]->rave_count += 1;
            }
		}
	}

    bool isEndBoard (board presentBoard, board::piece_type color) {
        for (int i = 0; i < CHILDNODESIZE; ++i) {
				board after = presentBoard;
				if (action::place(i, color).apply(after) == board::legal) {
					presentBoard.setBoard(i, color);
                    return false;
				}
		}
        return true;
    }

	inline void playOneSequence (node_t *rootNode, board presentBoard, std::vector<int> indexs) {
		node_t *selectNode[CHILDNODESIZE] = {NULL};
		selectNode[0] = rootNode;
		int i = 0;
        int move[CHILDNODESIZE + 1];
		while (!(selectNode[i]->isLeaf)) {
			selectNode[i + 1] = select(selectNode[i], presentBoard, who, move, i, indexs);
			i++;
		}
		int value = simulation(presentBoard, selectNode[i]->color, who, indexs);
		updateValue(selectNode, value, i, isEndBoard(presentBoard, selectNode[i]->color), move);
	}

	int simulation (board presentBoard, board::piece_type present_color, board::piece_type true_color, std::vector<int> indexs) {
		while (1) {
			bool flag = false;
            std::shuffle(indexs.begin(), indexs.end(), engine);

			for (int i = 0; i < CHILDNODESIZE; ++i) {
				board after = presentBoard;
				if (action::place(indexs[i], present_color).apply(after) == board::legal) {
					presentBoard.setBoard(indexs[i], present_color);
					flag = true;
					break;
				}
			}
			if (!flag)
				break;
			present_color = present_color == board::piece_type::white ? board::piece_type::black : board::piece_type::white; 
            presentBoard.change_turn();
		}
		if (present_color == true_color)
			return 0;
		return 1;
	}

    void free_tree (node_t *root) {
        if (root->isLeaf)
            free(root);
        else {
            for (int i = 0; i < CHILDNODESIZE; ++i)
                free_tree(root->child[i]);
        }
    }

    void show_board (board::grid stone) {
        for (int i = 0; i < 9; ++i) {
                for (int j = 0; j < 9; ++j) {
                    printf("%u ", stone[i][j]);
                }
                printf("\n");
        }
        printf("\n\n");
    }

private:
	std::vector<action::place> space;
	board::piece_type who;
    int simulation_times;
};
