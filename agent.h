/**
 * Framework for NoGo and similar games (C++ 11)
 * agent.h: Define the behavior of variants of the player
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <cstring>
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <time.h>
#include <stdlib.h>
#include "board.h"
#include "action.h"
#include <stdio.h>
#include <thread>
#include <queue>
#include <unistd.h>

#define CHILDNODESIZE 81
#define _b 0.025
#define COLLECTNODESIZE 250000
#define TREESIZE 50000
#define NUMOFTHREAD 2

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
typedef struct node {
	struct node *child[CHILDNODESIZE];
	int count, val, rave_count, rave_val;
	bool isLeaf;
	board::piece_type color;
} node_t;

typedef struct func_para {
	int index_of_tree;
	board state;
} func_para_t;

node_t *collectNode;

class player : public random_agent {
public:
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
		
		collectNode = (node_t *) malloc(sizeof(node_t) * COLLECTNODESIZE);
		memset(collectNode, 0, sizeof(node_t) * COLLECTNODESIZE);
	}

    float beta (int count, int rave_count) {
	    return (float) rave_count / ((float) rave_count + (float) count + 4 * (float) rave_count * (float) count * pow((_b), 2));
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

			// use root parallel mcts
			std::thread threads[NUMOFTHREAD];
			func_para_t para[NUMOFTHREAD]; 
			for (int i = 0; i < NUMOFTHREAD; ++i) {
				para[i].index_of_tree = i;
				para[i].state = state;
				threads[i] = std::thread(&player::child, this, para + i);
			}
			for (int i = 0; i < NUMOFTHREAD; ++i)
				threads[i].join();

			// compute the ucb of all child of each root
			float values[CHILDNODESIZE] = {0};
			for (int j = 0; j < NUMOFTHREAD; ++j) {
				node_t *root = collectNode + j * TREESIZE;
				for (int i = 0; i < CHILDNODESIZE; ++i) {
					node_t *cur = root->child[i];
					if (cur) {
						float q = (float) cur->val /  cur->count;
						float q_rave = (float) cur->rave_val /  cur->rave_count;
						float beta_ = beta(cur->count, cur->rave_count);
						values[i] += (1 - beta_) * q + beta_ * q_rave;
					}
				}
			}

			// find the max value of child
			int index = 0;
			for (int i = 1; i < CHILDNODESIZE; ++i)
				index = values[i] > values[index] ? i : index;

			return action::place(index, who);
		}
		return action();
	}

	bool select (node_t *parent, board& presentBoard, board::piece_type color, int move[CHILDNODESIZE + 1], int step, std::vector<int> indexs) {
		int total = parent->count;
        std::shuffle(indexs.begin(), indexs.end(), engine);

        bool isEndBoard = true, same_color = parent->color == color;
		float max = same_color ? -1 : 1.2e30;
        int max_op = 0;
		for (int i = 0; i < CHILDNODESIZE; ++i) {
			board after = presentBoard;
            node_t *child = parent->child[indexs[i]];
			if (action::place(indexs[i], parent->color).apply(after) == board::legal) {
                isEndBoard = false;
				if (!child) {
					max_op = indexs[i];
					break;
				}
                float q = (float) child->val /  child->count;
                float q_rave = (float) child->rave_val /  child->rave_count;
                float beta_ = beta(child->count, child->rave_count);
				float value = same_color ? \
							(1 - beta_) * q + beta_ * q_rave + pow(2 * log10(total) / child->count, 0.5) : \
							(1 - beta_) * q + beta_ * q_rave - pow(2 * log10(total) / child->count, 0.5);
                if ((same_color && value > max) || ((!same_color) && value < max)) {
                    max = value;
                    max_op = indexs[i];
                }
			}
		}

        if (isEndBoard)
            return true;

		presentBoard.setBoard(max_op, parent->color);
        move[step] = max_op;
        // change turn
        presentBoard.change_turn();
        return false;
	}

	inline void updateValue (node_t *rootNode, int value, int last, bool isEndBoard, int move[CHILDNODESIZE + 1], int& nodeCount, int index_of_tree) {
		node_t *curNode = rootNode, *lastNode = NULL;
		last -= isEndBoard;
		for (int i = 0; i < last; ++i) {
            for (int j = i + 2; j < last; j += 2) {
                curNode->child[move[j]]->rave_val += value;
                curNode->child[move[j]]->rave_count += 1;
            }
			curNode->val += value;
			curNode->count += 1;
			curNode->rave_val += value;
			curNode->rave_count += 1;
			lastNode = curNode;
			curNode = curNode->child[move[i]];
		}
        if (!isEndBoard) {
		    lastNode->child[move[last - 1]] = collectNode + nodeCount + index_of_tree * TREESIZE;
			nodeCount += 1;
			node_t *p = lastNode->child[move[last - 1]];
			p->color = board::piece_type(3 - lastNode->color);
			p->val = p->rave_val = value;
			p->count = p->rave_count = 1;
			memset(p->child, 0, CHILDNODESIZE * sizeof(node_t *));
		}
	}
    
	inline void playOneSequence (node_t *rootNode, board presentBoard, std::vector<int> indexs, int& nodeCount, int index_of_tree) {
		int i = 0, move[CHILDNODESIZE + 1] = {0};
		node_t *curNode = rootNode;
        bool isEndBoard = false;
		while (curNode && !isEndBoard) {
			isEndBoard = select(curNode, presentBoard, who, move, i, indexs);
			curNode = curNode->child[move[i]];
			i++;
		}
		int value = isEndBoard ? presentBoard.getWhoTakeTurns() != who : simulation(presentBoard, presentBoard.getWhoTakeTurns(), who, indexs);
		updateValue(rootNode, value, i, isEndBoard, move, nodeCount, index_of_tree);
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
			present_color = board::piece_type(3 - present_color);
            presentBoard.change_turn();
		}
		return present_color != true_color;
	}

	void run_mcts (int index_of_tree, const board state) {
		int nodeCount = 0;
    	std::vector<int> indexs;
        for (int i = 0; i < CHILDNODESIZE; ++i)
            indexs.push_back(i);
		std::shuffle(indexs.begin(), indexs.end(), engine);

		node_t *root = collectNode + index_of_tree * TREESIZE;
		nodeCount += 1;
		// root node value is don't care
		root->val = root->rave_val = 1;
		root->count = root->rave_count = 0;
		root->color = who;
		memset(root->child, 0, CHILDNODESIZE * sizeof(node_t *));

		clock_t start = clock();
		while (clock() - start < 980000)
			playOneSequence(root, state, indexs, nodeCount, index_of_tree);
		printf("index of tree: %d , root count: %d\n", index_of_tree, root->count);
	}

	void child(void *arg) {
		func_para_t *para = (func_para_t *) arg;
		run_mcts(para->index_of_tree, para->state);
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
    int simulation_times;
	// int nodeCount;
};


