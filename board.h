/**
 * Framework for NoGo and similar games (C++ 11)
 * board.h: Define the game state and basic operations of the game of NoGo
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <array>
#include <list>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <utility>
#include <cmath>

/**
 * definition for the 9x9 board
 * note that there is no column 'I'
 *
 *   A B C D E F G H J
 * 9 + + + + + + + + + 9
 * 8 + + + +   + + + + 8
 * 7 + + + +   + + + + 7
 * 6 + + + + + + + + + 6
 * 5 +     + + +     + 5
 * 4 + + + + + + + + + 4
 * 3 + + + +   + + + + 3
 * 2 + + + +   + + + + 2
 * 1 + + + + + + + + + 1
 *   A B C D E F G H J
 *
 * GTP style is operated as (move):
 *   "A1", "B3", ..., "H4", ..., "J9"
 * 1-d array style is operated as (i):
 *   (0) == "A1", (11) == "B3", ..., (66) == "H4", (80) == "J9"
 * 2-d array style is operated as [x][y]:
 *   [0][0] == "A1", [1][2] == "B3", [7][3] == "H4", [8][8] == "J9"
 *
 * for 9x9 Hollow NoGo, the empty locations are hollow but not empty, cannot be counted as liberty,
 * i.e., there are also borders at the center of the board
 */
class board {
public:
	enum size { size_x = 9u, size_y = 9u, hollow_x = 3u, hollow_y = 3u };
	enum piece_type { empty = 0u, black = 1u, white = 2u, hollow = 3u, unknown = -1u };
	typedef uint32_t cell;
	typedef std::array<cell, size_y> column;
	typedef std::array<column, size_x> grid;
	struct data {
		piece_type who_take_turns;
	};
	typedef uint64_t score;
	typedef int reward;

public:
	board() : stone(initial()), attr({piece_type::black}) {}
	board(const grid& b, const data& d) : stone(b), attr(d) {}
	board(const board& b) = default;
	board& operator =(const board& b) = default;

	struct point {
		int x, y, i;
		point(int i = -1) : x(i != -1 ? i / size_y : -1), y(i != -1 ? i % size_y : -1), i(i) {}
		point(int x, int y) : x(x), y(y), i(x != -1 && y != -1 ? x * size_y + y : -1) {}
		point(const std::string& name) : point(
			name.size() >= 2 && name != "PASS" ? name[0] - (name[0] > 'I' ? 'B' : 'A') : -1,
			name.size() >= 2 && std::isdigit(name[1]) ? std::stoul(name.substr(1)) - 1 : -1) {}
		point(const char* name) : point(std::string(name)) {}
		point(const point&) = default;
		operator std::string() const {
			if (i == -1) return "PASS";
			if (x >= size_x || y >= size_y) return "??";
			return std::string(1, x + (x < 8 ? 'A' : 'B')) + std::to_string(y + 1);
		}
	};

	operator grid&() { return stone; }
	operator const grid&() const { return stone; }
	column& operator [](unsigned x) { return stone[x]; }
	const column& operator [](unsigned x) const { return stone[x]; }
	cell& operator ()(unsigned i) { point p(i); return stone[p.x][p.y]; }
	const cell& operator ()(unsigned i) const { point p(i); return stone[p.x][p.y]; }
	cell& operator ()(const std::string& move) { point p(move); return stone[p.x][p.y]; }
	const cell& operator ()(const std::string& move) const { point p(move); return stone[p.x][p.y]; }

	data info() const { return attr; }
	data info(data dat) { data old = attr; attr = dat; return old; }

public:
	bool operator ==(const board& b) const { return stone == b.stone; }
	bool operator < (const board& b) const { return stone <  b.stone; }
	bool operator !=(const board& b) const { return !(*this == b); }
	bool operator > (const board& b) const { return b < *this; }
	bool operator <=(const board& b) const { return !(b < *this); }
	bool operator >=(const board& b) const { return !(*this < b); }

public:
	enum nogo_move_result {
		legal = reward(0),
		illegal_turn = reward(-1),
		illegal_pass = reward(-2),
		illegal_out_of_range = reward(-3),
		illegal_not_empty = reward(-4),
		illegal_suicide = reward(-5),
		illegal_take = reward(-6),
	};

	/**
	 * place a stone to the specific position
	 * who == piece_type::unknown indicates automatically play as the next side
	 * return nogo_move_result::legal if the action is valid, or nogo_move_result::illegal_* if not
	 */
	reward place(int x, int y, unsigned who = piece_type::unknown) {
		if (who == -1u) who = attr.who_take_turns;
		if (who != attr.who_take_turns) return nogo_move_result::illegal_turn;
		if (x == -1 && y == -1) return nogo_move_result::illegal_pass;
		point p_min(0, 0), p_max(size_x - 1, size_y - 1);
		if (x < p_min.x || x > p_max.x || y < p_min.y || y > p_max.y) return nogo_move_result::illegal_out_of_range;
		if (board::initial()[x][y] == piece_type::hollow)             return nogo_move_result::illegal_out_of_range;
		board test = *this;
		if (test[x][y] != piece_type::empty) return nogo_move_result::illegal_not_empty;
		test[x][y] = who; // try put a piece first
		if (test.check_liberty(x, y, who) == 0) return nogo_move_result::illegal_suicide;
		unsigned opp = 3u - who;
		if (x > p_min.x && test.check_liberty(x - 1, y, opp) == 0) return nogo_move_result::illegal_take;
		if (x < p_max.x && test.check_liberty(x + 1, y, opp) == 0) return nogo_move_result::illegal_take;
		if (y > p_min.y && test.check_liberty(x, y - 1, opp) == 0) return nogo_move_result::illegal_take;
		if (y < p_max.y && test.check_liberty(x, y + 1, opp) == 0) return nogo_move_result::illegal_take;
		stone[x][y] = who; // is legal move!
		attr.who_take_turns = static_cast<piece_type>(opp);
		return nogo_move_result::legal;
	}
	reward place(const point& p, unsigned who = piece_type::unknown) {
		return place(p.x, p.y, who);
	}

	/**
	 * calculate the liberty of the block of piece at [x][y]
	 * return >= 0 if [x][y] is placed by who; otherwise return -1
	 */
	int check_liberty(int x, int y, unsigned who) const {
		grid test = stone;
		if (test[x][y] != who) return -1;

		int liberty = 0;
		std::list<point> check;
		for (check.emplace_back(x, y); check.size(); check.pop_front()) {
			int x = check.front().x, y = check.front().y;
			test[x][y] = piece_type::unknown; // prevent recalculate

			point p_min(0, 0), p_max(size_x - 1, size_y - 1);

			cell near_l = x > p_min.x ? test[x - 1][y] : -1u; // left
			if (near_l == piece_type::empty) liberty++;
			else if (near_l == who) check.emplace_back(x - 1, y);

			cell near_r = x < p_max.x ? test[x + 1][y] : -1u; // right
			if (near_r == piece_type::empty) liberty++;
			else if (near_r == who) check.emplace_back(x + 1, y);

			cell near_d = y > p_min.y ? test[x][y - 1] : -1u; // down
			if (near_d == piece_type::empty) liberty++;
			else if (near_d == who) check.emplace_back(x, y - 1);

			cell near_u = y < p_max.y ? test[x][y + 1] : -1u; // up
			if (near_u == piece_type::empty) liberty++;
			else if (near_u == who) check.emplace_back(x, y + 1);
		}
		return liberty;
	}

	void transpose() {
		for (int x = 0; x < size_x; x++) {
			for (int y = x + 1; y < size_y; y++) {
				std::swap(stone[x][y], stone[y][x]);
			}
		}
	}

	void reflect_horizontal() {
		for (int y = 0; y < size_y; y++) {
			for (int x = 0; x < size_x / 2; x++) {
				std::swap(stone[x][y], stone[size_x - 1 - x][y]);
			}
		}
	}

	void reflect_vertical() {
		for (int x = 0; x < size_x; x++) {
			for (int y = 0; y < size_y / 2; y++) {
				std::swap(stone[x][y], stone[x][size_y - 1 - y]);
			}
		}
	}

	/**
	 * rotate the board clockwise by given times
	 */
	void rotate(int r = 1) {
		switch (((r % 4) + 4) % 4) {
		default:
		case 0: break;
		case 1: rotate_right(); break;
		case 2: reverse(); break;
		case 3: rotate_left(); break;
		}
	}

	void rotate_right() { transpose(); reflect_vertical(); } // clockwise
	void rotate_left() { transpose(); reflect_horizontal(); } // counterclockwise
	void reverse() { reflect_horizontal(); reflect_vertical(); }

	grid getStone() {
		return stone;
	}

	grid getAttr() {
		return attr;
	}

public:
	friend std::ostream& operator <<(std::ostream& out, const board& b) {
		std::ios ff(nullptr);
		ff.copyfmt(out); // make a copy of the original print format

		const char* axis_x_label = "ABCDEFGHJKLMNOPQRST?";
		int width_y = size_y < 10 ? 1 : 2;

		out << std::setw(width_y) << ' ';
		for (int x = 0; x < size_x; x++)
			out << ' ' << axis_x_label[std::min(x, 19)];
		out << ' ' << std::setw(width_y) << ' ' << std::endl;

		// for displaying { space, black, white }
		const char* print[] = {"\u00B7" /* or \u00A0 */, "\u25CF", "\u25CB", "\u00A0", "?"};
		for (int y = size_y - 1; y >= 0; y--) {
			out << std::right << std::setw(width_y) << (y + 1);
			for (int x = 0; x < size_x; x++)
				out << ' ' << print[std::min(b[x][y], 4u)];
			out << ' ' << std::left << std::setw(width_y) << (y + 1) << std::endl;
		}

		out << std::setw(width_y) << ' ';
		for (int x = 0; x < size_x; x++)
			out << ' ' << axis_x_label[std::min(x, 19)];
		out << ' ' << std::setw(width_y) << ' ' << std::endl;

		out.copyfmt(ff); // restore print format
		return out;
	}
	friend std::istream& operator >>(std::istream& in, board& b) {
		std::string token;
		for (int x = 0; x < size_x; x++) in >> token; /* skip X */
		for (int y = size_y - 1; y >= 0 && in >> token /* skip Y */; in >> token /* skip Y */, y--) {
			for (int x = 0; x < size_x && in >> token /* read a piece */; x++) {
				const char* print[] = {"\u00B7" /* or \u00A0 */, "\u25CF", "\u25CB", "\u00A0"};
				int type = -1;
				for (int i = 0; type == -1 && i < 4; i++) {
					if (token == print[i]) type = i;
				}
				if (type != -1) {
					b[x][y] = static_cast<piece_type>(type);
				} else {
					in.setstate(std::ios_base::failbit);
					return in;
				}
			}
		}
		for (int x = 0; x < size_x; x++) in >> token; /* skip X */
		return in;
	}
	friend std::ostream& operator <<(std::ostream& out, const point& p) {
		return out << std::string(p);
	}
	friend std::istream& operator >>(std::istream& in, point& p) {
		std::string name;
		if (in >> name) p = point(name);
		return in;
	}

protected:
	static const grid& initial() { static grid stone; return stone; }
	static __attribute__((constructor)) void init_initial_scheme() {
		grid& stone = const_cast<grid&>(initial());
		stone[4][1] = piece_type::hollow;
		stone[4][2] = piece_type::hollow;
		stone[4][6] = piece_type::hollow;
		stone[4][7] = piece_type::hollow;
		stone[1][4] = piece_type::hollow;
		stone[2][4] = piece_type::hollow;
		stone[6][4] = piece_type::hollow;
		stone[7][4] = piece_type::hollow;
	}
private:
	grid stone;
	data attr;
};
