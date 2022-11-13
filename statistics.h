/**
 * Framework for NoGo and similar games (C++ 11)
 * statistics.h: Utility for making statistical reports
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <deque>
#include <algorithm>
#include <iostream>
#include <sstream>
#include "board.h"
#include "action.h"
#include "episode.h"

class statistics {
public:
	/**
	 * the total episodes to run
	 * the block size of statistics
	 * the limit of saving records
	 *
	 * note that total >= limit >= block
	 */
	statistics(size_t total, size_t block = 0, size_t limit = 0)
		: total(total),
		  block(block ? block : total),
		  limit(limit ? limit : total),
		  count(0) {}

public:
	/**
	 * show the statistics of last 'block' games
	 *
	 * the format is
	 * 1000   win = 53.5%|46.5%, op = 74.451 (37.493|36.958), ops = 125762 (132018|135377)
	 *
	 * where (block = 1000 by default)
	 *  '1000': current index (n), i.e., this line is the statistic of game 1 ~ 1000
	 *  'win = 53.5%|46.5%': the win rate for black is 53.5%; for white is 46.5%
	 *  'op = 74.451 (37.493|36.958)': the average move is 74.451
	 *                                 the average move of black is 37.493
	 *                                 the average move of white is 36.958
	 *  'ops = 125762 (132018|135377)': the average speed is 125762
	 *                                  the average speed of black is 132018
	 *                                  the average speed of white is 135377
	 */
	void show(size_t blk = 0) const {
		size_t num = std::min(data.size(), blk ?: block);
		size_t sop = 0, Bop = 0, Wop = 0;
		time_t sdu = 0, Bdu = 0, Wdu = 0;
		size_t BW = 0, WW = 0;
		auto it = data.end();
		for (size_t i = 0; i < num; i++) {
			auto& ep = *(--it);
			if (ep.step() % 2 == 1) BW++;
			else                    WW++;
			sop += ep.step();
			Bop += ep.step(action::black::type);
			Wop += ep.step(action::white::type);
			sdu += ep.time();
			Bdu += ep.time(action::black::type);
			Wdu += ep.time(action::white::type);
		}

		std::cout << count << "\t";
		std::cout << "win = " << (BW * 100.0 / num) << "%"
		          <<      "|" << (WW * 100.0 / num) << "%, ";
		std::cout << "op = "  << (sop * 1.0 / num)
		          <<     " (" << (Bop * 1.0 / num)
		          <<      "|" << (Wop * 1.0 / num) << "), ";
		std::cout << "ops = " << (sop * 1000.0 / sdu)
		          <<     " (" << (Bop * 1000.0 / Bdu)
		          <<      "|" << (Wop * 1000.0 / Wdu) << ")";
		std::cout << std::endl;
	}

	void summary() const {
		show(data.size());
	}

	bool is_finished() const {
		return count >= total;
	}

	bool is_episode_ongoing() const {
		return data.size() && data.back().time() < 0;
	}

	void open_episode(const std::string& flag = "") {
		if (count++ >= limit) data.pop_front();
		data.emplace_back();
		data.back().open_episode(flag);
	}

	void close_episode(const std::string& flag = "") {
		data.back().close_episode(flag);
		if (count % block == 0) show();
	}

	episode& at(size_t i) {
		return data.at(i);
	}
	episode& front() {
		return data.front();
	}
	episode& back() {
		return data.back();
	}
	size_t step() const {
		return count;
	}

	friend std::ostream& operator <<(std::ostream& out, const statistics& stat) {
		for (const episode& rec : stat.data) out << rec << std::endl;
		return out;
	}
	friend std::istream& operator >>(std::istream& in, statistics& stat) {
		for (std::string line; std::getline(in, line) && line.size(); ) {
			stat.data.emplace_back();
			std::stringstream(line) >> stat.data.back();
		}
		stat.total = std::max(stat.total, stat.data.size());
		stat.count = stat.data.size();
		return in;
	}

private:
	size_t total;
	size_t block;
	size_t limit;
	size_t count;
	std::deque<episode> data;
};
