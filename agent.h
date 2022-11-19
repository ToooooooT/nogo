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
#define SIMULATION_TIMES 1000
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
		int count, *val, rave_count, *rave_val;
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

    double beta(int count, int rave_count) {
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
            // if no legal move just return
            bool flag = false;
			for (const action::place& move : space) {
				board after = state;
				if (move.apply(after) == board::legal) {
                    flag = true;
                    break;
                }
            }
            if (!flag)
                return action();

            // create root of MCTS tree
			node_t *root = (node_t *) malloc (sizeof(node_t));
			root->isLeaf = true;
			root->count = root->rave_count = 0;
			root->color = who;
            // root count = 1000, 1024 becuase * 2
            root->val = (int *) malloc (sizeof(int) * 1024);
            root->rave_val = (int *) malloc (sizeof(int) * 1024);
			for (int i = 0; i < simulation_times; ++i)
				playOneSequence(root, state);
            
            // shuffle index to choose random move
            std::vector<int> indexs;
            for (int i = 0; i < CHILDNODESIZE; ++i)
                indexs.push_back(i);
            std::shuffle(indexs.begin(), indexs.end(), engine);

			int index = 0;
            while (!(root->child[indexs[index]]->count))
                index += 1;
			for (int i = index + 1; i < CHILDNODESIZE; ++i) {
                int count = root->child[indexs[i]]->count;
                int value = root->child[indexs[i]]->val[count - 1];
                int max_count = root->child[indexs[index]]->count;
                int max_value = root->child[indexs[index]]->val[max_count - 1];
                int rave_count = root->child[indexs[i]]->rave_count;
                int rave_value = root->child[indexs[i]]->rave_val[rave_count - 1];
                int max_rave_count = root->child[indexs[index]]->rave_count;
                int max_rave_value = root->child[indexs[index]]->rave_val[max_rave_count - 1];
                if (root->child[indexs[i]]->count)
    				index = (1 - beta(count, rave_count)) * (double) value  / count + beta(count, rave_count) * (double) rave_value / rave_count > (1 - beta(max_count, max_rave_count)) * (double) max_value  / max_count + beta(max_count, max_rave_count) * (double) max_rave_value / max_rave_count ? i : index;
            }

            // clear MCTS search tree
            free_tree(root);

            /*
            board::grid stone = board(state).getStone();
            show_board(stone);
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

    double UCB (int val, int count, int total) {
        return (double) val / count + pow(2 * log10(total) / count, 0.5);
    }

    double UCB_Tuned (node_t *node, int total) {
        double mean =  (double) node->val[node->count - 1] / node->count;
        double rave_mean =  (double) node->rave_val[node->rave_count - 1] / node->rave_count;
        double value = - pow(mean, 2) + pow(2 * log10(total) / node->count, 0.5);
        double sum = 0;
        for (int i = 0; i < node->count; ++i)
            sum += pow(node->val[i], 2);
        sum /= node->count;
        value += sum;
        double min = 0.25 < value ? 0.25 : value;
        return (1 - beta(node->count, node->rave_count)) * mean + beta(node->count, node->rave_count) * rave_mean + pow(log10(total) * min / node->count, 0.5);
    }

	node_t *select (node_t *parent, board& presentBoard, board::piece_type color) {
		int total = 0;
		double v[CHILDNODESIZE] = {0.0};
        // calculate total count 
		for (int i = 0; i < CHILDNODESIZE; ++i)
			total += parent->child[i]->count;

        // calculate each UCB of childs' node
		for (int i = 0; i < CHILDNODESIZE; ++i) {
			board after = presentBoard;
			if (action::place(i, parent->color).apply(after) == board::legal) {
				if (parent->color == color)
					v[i] = parent->child[i]->count == 0 ? 1e308 : UCB_Tuned(parent->child[i], total);
				else
					v[i] = parent->child[i]->count == 0 ? 0 : UCB_Tuned(parent->child[i], total);
			} else
				v[i] = parent->color == color ? -1 : 1.2e308;
		}

        // shuffle index to choose random child
        std::vector<int> indexs;
        for (int i = 0; i < CHILDNODESIZE; ++i)
            indexs.push_back(i);
        std::shuffle(indexs.begin(), indexs.end(), engine);

		int i = 0;
        while (parent->color == color && v[indexs[i]] == -1.0)
            i += 1;
        while (parent->color != color && v[indexs[i]] == 1.2e308)
            i += 1;
		for (int j = i + 1; j < CHILDNODESIZE; ++j) {
            // choose max value when match color otherwise min value
			if (parent->color == color)
				i = v[indexs[j]] > v[indexs[i]] ? j : i;
			else
				i = v[indexs[j]] < v[indexs[i]] ? j : i;
		}
		presentBoard.setBoard(indexs[i], parent->color);

        int tmp = indexs[i];
        indexs.clear();

        // change turn
        presentBoard.change_turn();

		return parent->child[tmp];
	}

	void updateValue (node_t *selectNode[CHILDNODESIZE], int value, int last, bool isEndBoard) {
		node_t *p = selectNode[last];

        // store every move in selected path 
        int move[last];

        if (!isEndBoard) {
		    p->isLeaf = false;
    		for (int i = 0; i < CHILDNODESIZE; ++i) {
	    		p->child[i] = (node_t *) malloc (sizeof(node_t));
		    	p->child[i]->isLeaf = true;
			    if (p->color == board::piece_type::black)
				    p->child[i]->color = board::piece_type::white;
    			else
	    			p->child[i]->color = board::piece_type::black;
		    	p->child[i]->count = 0;
    		}   
        }
		for (int i = last; i >= 0; --i) {
            // store select move
            if (i < last) {
                for (int j = 0; j < CHILDNODESIZE; ++j) {
                    if (selectNode[i]->child[j] == selectNode[i + 1]) {
                        move[i] = j;
                        break;
                    }
                }
            }
            
            // update by rave
            for (int j = last - 1; j > i; --j) {
                if (__builtin_popcount(selectNode[j]->rave_count) == 1)
                    selectNode[j]->rave_val = (int *) realloc (selectNode[j]->rave_val, (selectNode[j]->rave_count << 1) * sizeof(int));
    			selectNode[j]->rave_val[selectNode[j]->rave_count] = selectNode[j]->rave_val[selectNode[j]->rave_count - 1] + value;
			    selectNode[j]->rave_count += 1;
            }

            if (selectNode[i]->count == 0)  {
                selectNode[i]->val = (int *) malloc (1 * sizeof(int));
    			selectNode[i]->val[selectNode[i]->count] = value;
                selectNode[i]->rave_val = (int *) malloc (1 * sizeof(int));
    			selectNode[i]->rave_val[selectNode[i]->rave_count] = value;
            } else {
                // simple
                if (__builtin_popcount(selectNode[i]->count) == 1)
                    selectNode[i]->val = (int *) realloc (selectNode[i]->val, (selectNode[i]->count << 1) * sizeof(int));
    			selectNode[i]->val[selectNode[i]->count] = selectNode[i]->val[selectNode[i]->count - 1] + value;

                // rave
                if (__builtin_popcount(selectNode[i]->rave_count) == 1)
                    selectNode[i]->rave_val = (int *) realloc (selectNode[i]->rave_val, (selectNode[i]->rave_count << 1) * sizeof(int));
    			selectNode[i]->rave_val[selectNode[i]->rave_count] = selectNode[i]->rave_val[selectNode[i]->rave_count - 1] + value;
            }
			selectNode[i]->count += 1;
			selectNode[i]->rave_count += 1;
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

	void playOneSequence (node_t *rootNode, board presentBoard) {
		node_t *selectNode[CHILDNODESIZE] = {NULL};
		selectNode[0] = rootNode;
		int i = 0;
		while (!(selectNode[i]->isLeaf)) {
			selectNode[i + 1] = select(selectNode[i], presentBoard, who);
			i++;
		}
		int value = simulation(presentBoard, selectNode[i]->color, who);
		updateValue(selectNode, value, i, isEndBoard(presentBoard, selectNode[i]->color));
	}

	int simulation (board presentBoard, board::piece_type present_color, board::piece_type true_color) {
        std::vector<int> indexs;
        for (int i = 0; i < CHILDNODESIZE; ++i)
            indexs.push_back(i);

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
			if (present_color == board::piece_type::white)
				present_color = board::piece_type::black; 
			else
				present_color = board::piece_type::white; 
            presentBoard.change_turn();
		}
        indexs.clear();
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
                for (int j = 0; j < 9; ++j)
                    printf("%u ", stone[i][j]);
                printf("\n");
        }
        printf("\n\n");
    }

private:
	std::vector<action::place> space;
	board::piece_type who;
    int simulation_times;
};

