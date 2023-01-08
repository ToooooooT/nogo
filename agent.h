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
#define COLLECTNODESIZE 200000
#define TREESIZE 50000

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
	virtual std::string thread_count() const { return property("thread"); }
	virtual std::string self_round() const { return property("round"); }

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
			thread_num = stoi(thread_count());
		
		collectNode = (node_t *) malloc(sizeof(node_t) * COLLECTNODESIZE);
		memset(collectNode, 0, sizeof(node_t) * COLLECTNODESIZE);
	}

	void set_timestep (int count) {
		double sum = 0, var = 3, mean = 14, prob[40];
		for (int i = count - 1; i < 40; ++i) {
			prob[i] = (1 / (var * pow(2 * 3.14159, 0.5)) * exp(-pow(i - mean, 2) / (var * var * 2)));
			sum += prob[i];
		}
		for (int i = count - 1; i < 40; i++)
			timestep[i] = 0.016 + 1 * prob[i] / sum;
	}

    float beta (int count, int rave_count) {
	    return (float) rave_count / ((float) rave_count + (float) count + 4 * (float) rave_count * (float) count * pow((_b), 2));
    }

	int hueristic_pos (board state) {
		/*
		 *  return position > 0 if legal move else -1 
		 */

		/* edge: 
		 * 	up : 3, 5 
		 * 	left : 27, 45
		 * 	right : 35, 53 
		 * 	down : 75, 77
		 */
		int ret = -1;
		bool retIsEye = false;
		const int edge_pos[8] = {3, 5, 27, 45, 35, 53, 75, 77};
		for (int i = 0; i < 8; ++i) {
			board after = state;
			if (action::place(edge_pos[i], who).apply(after) == board::legal) {
				if (makeEye(state, edge_pos[i] / 9, edge_pos[i] % 9, who) >= 0) {
					retIsEye = true;
					ret = edge_pos[i];
				}
				else if (ret == -1)
					ret = edge_pos[i];
			}
		}

		/* corner: 
		 * 	left up : 1, 9 
		 * 	right up : 7, 17
		 * 	left down : 63, 73 
		 * 	right down : 71, 79
		 */
		const int corner_pos[8] = {1, 9, 7, 17, 63, 73, 71, 79};
		for (int i = 0; i < 8; ++i) {
			board after = state;
			if (action::place(corner_pos[i], who).apply(after) == board::legal && ((makeEye(state, corner_pos[i] / 9, corner_pos[i] % 9, who) >= 0 && !retIsEye) || ret == -1))
				ret = corner_pos[i];
		}

		if (ret == -1)
			return ret;

		/*
		 *  if oponent can make eye then break it.
		 */
		state.change_turn();
		for (int i = 0; i < 81; ++i) {
			board after = state;
			if (action::place(i, after.getWhoTakeTurns()).apply(after) == board::legal) {
				int eye_pos = makeEye(state, i / 9, i % 9, state.getWhoTakeTurns());
				if (eye_pos >= 0)
					return eye_pos;
			}
		}

		return ret;
	}

	int makeEye (board state, int x, int y, board::piece_type color) {
		// return eye position if have eye
		board after = state;
		after.setBoard(x * 9 + y, color);
		if (isEye(after, x - 1, y, color))
			return (x - 1) * 9 + y;
		if (isEye(after, x + 1, y, color))
			return (x + 1) * 9 + y; 
		if (isEye(after, x, y - 1, color))
			return x * 9 + y - 1;
		if (isEye(after, x, y + 1, color))
			return x * 9 + y + 1;
		return -1;
	}

	bool isEye (board state, int x, int y, board::piece_type color) {
		// hollow : 13, 22, 37, 38, 42, 43, 58, 67
		if (x < 0 || x >= 9 || y < 0 || y >= 9 || state.getStone()[x][y] == board::piece_type::empty)
			return false;

		int left_index = x * 9 + y - 1;
		int right_index = x * 9 + y + 1;
		int down_index = (x + 1) * 9 + y;
		int up_index = (x - 1) * 9 + y;
		bool left = y - 1 < 0 || left_index == 13 || left_index == 22 || left_index == 37 || left_index == 38 || left_index == 42 || left_index == 43 || left_index == 58 || left_index == 67 || state.getStone()[x][y - 1] == color;
		bool right = y + 1 >= 9 || right_index == 13 || right_index == 22 || right_index == 37 || right_index == 38 || right_index == 42 || right_index == 43 || right_index == 58 || right_index == 67 || state.getStone()[x][y + 1] == color;
		bool down = x + 1 >= 9 || down_index == 13 || down_index == 22 || down_index == 37 || down_index == 38 || down_index == 42 || down_index == 43 || down_index == 58 || down_index == 67 || state.getStone()[x + 1][y] == color;
		bool up = x - 1 < 0 || up_index == 13 || up_index == 22 || up_index == 37 || up_index == 38 || up_index == 42 || up_index == 43 || up_index == 58 || up_index == 67 || state.getStone()[x - 1][y] == color;
		return left & right & down & up;
	}

	virtual action take_action(const board& state) {
		count_move += 1;
		if (search() == "Random") {
			std::shuffle(space.begin(), space.end(), engine);
			for (const action::place& move : space) {
				board after = state;
				if (move.apply(after) == board::legal)
					return move;
			}
			return action();
		} else if (search() == "MCTS") {
			// use hueristic
			int hue_pos;
			if (use_hue && count_move <= 8) {
				hue_pos = hueristic_pos(state);
				board after = state;
				if (hue_pos >= 0 && action::place(hue_pos, who).apply(after) == board::legal)
					return action::place(hue_pos, who);
				else
					use_hue = false;
			}
				
			// use root parallel mcts
			std::thread threads[thread_num];
			func_para_t para[thread_num]; 
			for (int i = 0; i < thread_num; ++i) {
				para[i].index_of_tree = i;
				para[i].state = state;
				threads[i] = std::thread(&player::child, this, para + i);
			}
			for (int i = 0; i < thread_num; ++i)
				threads[i].join();

			// compute the ucb of all child of each root
			float values[CHILDNODESIZE];
			for (int i = 0; i < CHILDNODESIZE; ++i)
				values[i] = -1;
			for (int j = 0; j < thread_num; ++j) {
				node_t *root = collectNode + j * TREESIZE;
				for (int i = 0; i < CHILDNODESIZE; ++i) {
					node_t *cur = root->child[i];
					if (cur) {
						if (values[i] == -1)
							values[i] = 0;
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

		struct timespec start, finish;
		double elapsed = 0;
		clock_gettime(CLOCK_MONOTONIC, &start);
		while (elapsed < 0.98) {
			playOneSequence(root, state, indexs, nodeCount, index_of_tree);
			clock_gettime(CLOCK_MONOTONIC, &finish);
			elapsed = finish.tv_sec - start.tv_sec;
			elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
		}
	}

	void child(void *arg) {
		func_para_t *para = (func_para_t *) arg;
		run_mcts(para->index_of_tree, para->state);
	}

	virtual void open_episode(const std::string& flag = "") {
		use_hue = true;
		count_move = 0;
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
	int thread_num;
	double timestep[40];
	bool use_hue;
	int count_move = 0;
};


