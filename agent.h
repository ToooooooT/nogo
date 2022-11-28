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

#define CHILDNODESIZE 81
#define _b 0.025
#define COLLECTNODESIZE 15000

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

        for (int i = 0; i < CHILDNODESIZE; ++i)
            indexs.push_back(i);
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

			collectNode = (node_t *) malloc(sizeof(node_t) * COLLECTNODESIZE);
			memset(collectNode, 0, sizeof(node_t) * COLLECTNODESIZE);
			nodeCount = 0;

            std::shuffle(indexs.begin(), indexs.end(), engine);

			node_t *root = collectNode + nodeCount;
			nodeCount += 1;
            root->val = root->rave_val = simulation(state, who, who, indexs);
			root->count = root->rave_count = 1;
			root->color = who;
            memset(root->child, 0, CHILDNODESIZE * sizeof(node_t *));
			clock_t start = clock();
			while (clock() - start < 900000)
				playOneSequence(root, state, indexs);

			int index = 0;
            while (!(root->child[indexs[index]]))
                index += 1;
            node_t *p = root->child[indexs[index]];
            double max_q = (double) p->val /  p->count;
            double max_q_rave = (double) p->rave_val / p->rave_count;
            double max_beta_ = beta(p->count, p->rave_count);
            double max = (1 - max_beta_) * max_q + max_beta_ * max_q_rave;
			for (int i = index + 1; i < CHILDNODESIZE; ++i) {
                node_t *cur = root->child[indexs[i]];
                if (cur) {
                    double q = (double) cur->val /  cur->count;
                    double q_rave = (double) cur->rave_val /  cur->rave_count;
                    double beta_ = beta(cur->count, cur->rave_count);
                    if ((1 - beta_) * q + beta_ * q_rave > max) {
    				    index = i;
                        max = (1 - beta_) * q + beta_ * q_rave;
                    }
                }
            }

            free(collectNode);

            /*
            board::grid stone = board(state).getStone();
            show_board(stone);
            printf("move index: %d\n", indexs[index]);
            */

			return action::place(indexs[index], who);
		}
		return action();
	}

	unsigned my_close_episode(const std::string& flag = "") {
		return flag == "black" ? 1u : 2u;
	}

	bool select (node_t *parent, board& presentBoard, board::piece_type color, int move[CHILDNODESIZE + 1], int step, std::vector<int> indexs) {
		int total = 0;
        std::shuffle(indexs.begin(), indexs.end(), engine);
		for (int i = 0; i < CHILDNODESIZE; ++i) {
			board after = presentBoard;
			if (!(parent->child[indexs[i]]) && action::place(indexs[i], parent->color).apply(after) == board::legal) {
				move[step] = indexs[i];
				presentBoard.setBoard(indexs[i], parent->color);
        		presentBoard.change_turn();
				return false;
			} else if (parent->child[indexs[i]])
				total += parent->child[indexs[i]]->count;
		}

		double max = parent->color == color ? -1 : 1.2e308;
        int max_op = 0;
        bool isEndBoard = true;
		for (int i = 0; i < CHILDNODESIZE; ++i) {
			board after = presentBoard;
            node_t *child = parent->child[indexs[i]];
			if (action::place(indexs[i], parent->color).apply(after) == board::legal) {
                double q = (double) child->val /  child->count;
                double q_rave = (double) child->rave_val /  child->rave_count;
                double beta_ = beta(child->count, child->rave_count);
                if (parent->color == color) {
                    if ((1 - beta_) * q + beta_ * q_rave + pow(2 * log10(total) / child->count, 0.5) > max) {
                        max = (1 - beta_) * q + beta_ * q_rave + pow(2 * log10(total) / child->count, 0.5);
                        max_op = indexs[i];
                    }
                } else {
                    if ((1 - beta_) * q + beta_ * q_rave + pow(2 * log10(total) / child->count, 0.5) < max) {
                        max = (1 - beta_) * q + beta_ * q_rave + pow(2 * log10(total) / child->count, 0.5);
                        max_op = indexs[i];
                    }

                }
                isEndBoard = false;
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

	inline void updateValue (node_t *rootNode, int value, int last, bool isEndBoard, int move[CHILDNODESIZE + 1]) {
		node_t *curNode = rootNode, *lastNode = NULL;
		last -= isEndBoard;
		for (int i = 0; i < last; ++i) {
            for (int j = i + 2; j < last; j += 2) {
                curNode->child[move[j]]->rave_val += value;
                curNode->child[move[j]]->rave_count += 1;
            }
			curNode->val += value;
			curNode->count += 1;
			lastNode = curNode;
			curNode = curNode->child[move[i]];
		}
        if (!isEndBoard) {
		    lastNode->child[move[last - 1]] = collectNode + nodeCount;
			nodeCount += 1;
			node_t *p = lastNode->child[move[last - 1]];
			p->color = lastNode->color == board::piece_type::black ? board::piece_type::white : board::piece_type::black;
			p->val = p->rave_val = value;
			p->count = p->rave_count = 1;
			memset(p->child, 0, CHILDNODESIZE * sizeof(node_t *));
		}
	}
    
	inline void playOneSequence (node_t *rootNode, board presentBoard, std::vector<int> indexs) {
		int i = 0;
        int move[CHILDNODESIZE + 1] = {0};
		node_t *curNode = rootNode;
        bool isEndBoard = false;
		while (curNode && !isEndBoard) {
			isEndBoard = select(curNode, presentBoard, who, move, i, indexs);
			curNode = curNode->child[move[i]];
			i++;
		}
		int value = simulation(presentBoard, presentBoard.getWhoTakeTurns(), who, indexs);
		updateValue(rootNode, value, i, isEndBoard, move);
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
			present_color = present_color == board::piece_type::black ? board::piece_type::white : board::piece_type::black;
            presentBoard.change_turn();
		}
		if (present_color == true_color)
			return 0;
		return 1;
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
    std::vector<int> indexs;
	node_t *collectNode;
	int nodeCount;
};
