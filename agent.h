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
#include "board.h"
#include "action.h"

#define CHILDNODESIZE 81

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
	// virtual std::string sim_time() const { return property("simulation"); }

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
		struct node *child[81];
		int count, val;
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
			node_t *root = (node_t *) malloc (sizeof(node_t));
			root->isLeaf = false;
			root->count = 0;
			root->val = 0;
			root->color = who;
			for (int i = 0; i < 100; ++i)
				playOneSequence(root, state);

			int index = 0;
			for (int i = 1; i < CHILDNODESIZE; ++i)
				index = (double) root->child[i]->val / root->child[i]->count > (double) root->child[index]->val / root->child[index]->count ? i : index;

			return action::place(index, who);
		}
		return action();
	}

	unsigned my_close_episode(const std::string& flag = "") {
		return flag == "black" ? 1u : 2u;
	}

	node_t *select (node_t *parent, board& presentBoard, board::piece_type color) {
		int total = 0;
		double v[CHILDNODESIZE] = {0.0};
		for (int i = 0; i < CHILDNODESIZE; ++i)
			total += parent->child[i]->count;
		for (int i = 0; i < CHILDNODESIZE; ++i) {
			board after = presentBoard;
			if (action::place(i, parent->color).apply(after) == board::legal) {
				if (parent->color == color)
					v[i] = parent->child[i]->count == 0 ? 1e308 : (double) parent->child[i]->val /  parent->child[i]->count + pow(2 * log10(total) / parent->child[i]->count, 0.5);
				else
					v[i] = parent->child[i]->count == 0 ? 0 : (double) parent->child[i]->val /  parent->child[i]->count + pow(2 * log10(total) / parent->child[i]->count, 0.5);
			} else
				v[i] = parent->color == color ? 0 : 1e308;
		}
		int index = 0;
		for (int i = 1; i < CHILDNODESIZE; ++i) {
			if (parent->color == 1u)
				index = v[i] > v[index] ? i : index;
			else
				index = v[i] < v[index] ? i : index;
		}
		presentBoard.setBoard(index, parent->color);
		return parent->child[index];
	}

	void updateValue (node_t *selectNode[CHILDNODESIZE], int value, int last) {
		node_t *p = selectNode[last];
		p->isLeaf = false;
		for (int i = 0; i <= last; ++i) {
			p->child[i] = (node_t *) malloc (sizeof(node_t));
			p->child[i]->isLeaf = true;
			if (p->color == board::piece_type::black)
				p->child[i]->color = board::piece_type::white;
			else
				p->child[i]->color = board::piece_type::black;
			p->child[i]->count = p->child[i]->val = 0;
		}
		for (int i = last; i >= 0; --i) {
			selectNode[i]->val += value;
			selectNode[i]->count += 1;
		}
	}

	void playOneSequence (node_t *rootNode, board presentBoard) {
		node_t *selectNode[CHILDNODESIZE] = {NULL};
		selectNode[0] = rootNode;
		int i = 0;
		while (!selectNode[i]->isLeaf) {
			selectNode[i + 1] = select(selectNode[i], presentBoard, who);
			i++;
		}
		int value = simulation(presentBoard, selectNode[i]->color, who);
		updateValue(selectNode, value, i);
	}

	int simulation (board presentBoard, board::piece_type present_color, board::piece_type true_color) {
		while (1) {
			bool flag = false;
			std::shuffle(space.begin(), space.end(), engine);
			for (const action::place& move : space) {
				board after = presentBoard;
				if (move.apply(after) == board::legal) {
					presentBoard.setBoard(move.pos, present_color);
					flag = true;
					break;
				}
			}
			if (!flag)
				break;
			if (present_color == board::piece_type::white)
				present_color = board::piece_type::white; 
			else
				present_color = board::piece_type::black; 
		}
		if (present_color == true_color)
			return 0;
		return 1;
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
};
