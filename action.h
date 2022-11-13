/**
 * Framework for NoGo and similar games (C++ 11)
 * action.h: Define the behavior of actions for the player
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <algorithm>
#include <unordered_map>
#include <string>
#include "board.h"

class action {
public:
	action(unsigned code = -1u) : code(code) {}
	action(const action& a) : code(a.code) {}
	virtual ~action() {}

	class place; // create a placing action with position and a color
	class black; // create a placing action of black with position
	class white; // create a placing action of white with position

public:
	virtual board::reward apply(board& b) const {
		auto proto = entries().find(type());
		if (proto != entries().end()) return proto->second->reinterpret(this).apply(b);
		return -1;
	}
	virtual std::ostream& operator >>(std::ostream& out) const {
		auto proto = entries().find(type());
		if (proto != entries().end()) return proto->second->reinterpret(this) >> out;
		return out << "??";
	}
	virtual std::istream& operator <<(std::istream& in) {
		auto state = in.rdstate();
		for (auto proto = entries().begin(); proto != entries().end(); proto++) {
			if (proto->second->reinterpret(this) << in) return in;
			in.clear(state);
		}
		return in.ignore(2);
	}

public:
	operator unsigned() const { return code; }
	unsigned type() const { return code & type_flag(-1u); }
	unsigned event() const { return code & ~type(); }
	friend std::ostream& operator <<(std::ostream& out, const action& a) { return a >> out; }
	friend std::istream& operator >>(std::istream& in, action& a) { return a << in; }

protected:
	static constexpr unsigned type_flag(unsigned v) { return v << 24; }

	typedef std::unordered_map<unsigned, action*> prototype;
	static prototype& entries() { static prototype m; return m; }
	virtual action& reinterpret(const action* a) const { return *new (const_cast<action*>(a)) action(*a); }

	unsigned code;
};

class action::place : public action {
public:
	static constexpr unsigned type = type_flag('p');
	place(int i, unsigned who) : action(place::type | ((who & 0xff) << 16) | (i & 0xffff)) {}
	place(int x, int y, unsigned who) : place(board::point(x, y), who) {}
	place(const board::point& p, unsigned who) : place(p.i, who) {}
	place(const action& a = {}) : action(a) {}
	board::point position() const { return board::point(int16_t(event() & 0xffff)); }
	board::piece_type color() const { return static_cast<board::piece_type>(event() >> 16); }
public:
	board::reward apply(board& b) const { return b.place(position(), color()); }
	std::ostream& operator >>(std::ostream& out) const {
		return out << ';' << "?BW?"[color() & 0b11] << '[' << char('a' + position().x)
		           << char('a' + ((board::size_y - 1) - position().y)) << ']';
	}
	std::istream& operator <<(std::istream& in) {
		while (isspace(in.peek()) && in.ignore(1));
		char buf[8];
		if (in.peek() == ';' && in.read(buf, 6)) { // ;B[aa]
			unsigned who = board::empty;
			if (buf[1] == 'B') who = board::black;
			if (buf[1] == 'W') who = board::white;
			int x = buf[3] - 'a';
			int y = (board::size_y - 1) - (buf[4] - 'a');
			operator=(place(x, y, who));
		} else {
			in.setstate(std::ios::failbit);
		}
		return in;
	}
protected:
	action& reinterpret(const action* a) const { return *new (const_cast<action*>(a)) place(*a); }
	static __attribute__((constructor)) void init() { entries()[type_flag('p')] = new place; }
};

class action::black : public action::place {
public:
	static constexpr unsigned type = type_flag('B');
	black(int x, int y) : action::place(x, y, board::black) {}
	black(int i) : action::place(i, board::black) {}
	black(const board::point& p) : action::place(p, board::black) {}
	black(const action& a = {}) : action::place(a) {}
protected:
	action& reinterpret(const action* a) const { return *new (const_cast<action*>(a)) black(*a); }
	static __attribute__((constructor)) void init() { entries()[type_flag('B')] = new black; }
};

class action::white : public action::place {
public:
	static constexpr unsigned type = type_flag('W');
	white(int x, int y) : action::place(x, y, board::white) {}
	white(int i) : action::place(i, board::white) {}
	white(const board::point& p) : action::place(p, board::white) {}
	white(const action& a = {}) : action::place(a) {}
protected:
	action& reinterpret(const action* a) const { return *new (const_cast<action*>(a)) white(*a); }
	static __attribute__((constructor)) void init() { entries()[type_flag('W')] = new white; }
};
