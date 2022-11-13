/**
 * Framework for NoGo and similar games (C++ 11)
 * nogo.cpp: Main file for the framework
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#include <iostream>
#include <fstream>
#include <iterator>
#include <string>
#include "board.h"
#include "action.h"
#include "agent.h"
#include "episode.h"
#include "statistics.h"

int main(int argc, const char* argv[]) {
	std::cout << "HollowNoGo-Demo: ";
	std::copy(argv, argv + argc, std::ostream_iterator<const char*>(std::cout, " "));
	std::cout << std::endl << std::endl;

	size_t total = 1000, block = 0, limit = 0;
	std::string black_args, white_args;
	std::string load_path, save_path;
	std::string name = "TCG-HollowNoGo-Demo", version = "2022"; // for GTP shell
	bool shell = false;
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		auto match_arg = [&](std::string flag) -> bool {
			auto it = arg.find_first_not_of('-');
			return arg.find(flag, it) == it;
		};
		auto next_opt = [&]() -> std::string {
			auto it = arg.find('=') + 1;
			return it ? arg.substr(it) : argv[++i];
		};
		if (match_arg("total")) {
			total = std::stoull(next_opt());
		} else if (match_arg("block")) {
			block = std::stoull(next_opt());
		} else if (match_arg("limit")) {
			limit = std::stoull(next_opt());
		} else if (match_arg("black")) {
			black_args = next_opt();
		} else if (match_arg("white")) {
			white_args = next_opt();
		} else if (match_arg("load")) {
			load_path = next_opt();
		} else if (match_arg("save")) {
			save_path = next_opt();
		} else if (match_arg("name")) {
			name = next_opt();
		} else if (match_arg("version")) {
			version = next_opt();
		} else if (match_arg("shell")) {
			shell = true;
		}
	}

	statistics stats(total, block, limit);

	if (load_path.size()) {
		std::ifstream in(load_path, std::ios::in);
		in >> stats;
		in.close();
		if (stats.is_finished()) stats.summary();
	}

	player black("name=black " + black_args + " role=black");
	player white("name=white " + white_args + " role=white");

	if (!shell) { // launch standard local games
		while (!stats.is_finished()) {
//			std::cerr << "======== Game " << stats.step() << " ========" << std::endl;
			black.open_episode("~:" + white.name());
			white.open_episode(black.name() + ":~");

			stats.open_episode(black.name() + ":" + white.name());
			episode& game = stats.back();
			while (true) {
				agent& who = game.take_turns(black, white);
				action move = who.take_action(game.state());
//				std::cerr << game.state() << "#" << game.step() << " " << who.name() << ": " << move << std::endl;
				if (game.apply_action(move) != true) break;
				if (who.check_for_win(game.state())) break;
			}
			agent& win = game.last_turns(black, white);
			stats.close_episode(win.name());

			black.close_episode(win.name());
			white.close_episode(win.name());
		}
	} else { // launch GTP shell
		for (std::string command; std::getline(std::cin, command); ) {
			if (command.back() == '\r') command.pop_back();
			if (command.empty()) continue;

			std::vector<std::string> args;
			std::istringstream iss(command);
			for (std::string s; getline(iss, s, ' '); args.push_back(s));

			std::string reply;
			if (args[0] == "play" || args[0] == "genmove") { // play a move, or generate a move and play
				if (!stats.is_episode_ongoing()) { // should open an episode
					black.open_episode("~:" + white.name());
					white.open_episode(black.name() + ":~");
					stats.open_episode(black.name() + ":" + white.name());
				}

				episode& game = stats.back();
				agent& who = game.take_turns(black, white);
				if (who.role()[0] != std::tolower(args[1][0])) { // player mismatch?!
					std::cout << "= " << "resign" << std::endl << std::endl;
					// show the error message and terminate the shell
					std::cerr << "player color " << args[1] << " mismatch!" << std::endl;
					std::cerr << "current state, "
					          << who.role() << " to play: " << std::endl << game.state();
					break;
				}
				if (args[0] == "play") { // play a move
					std::string types = "?bw"; // black == 1, white == 2
					action::place move(args[2], types.find(who.role()[0]));
					if (game.apply_action(move) != true) { // remote plays an illegal move?!
						std::cout << "= " << "resign" << std::endl << std::endl;
						// show the error message and terminate the shell
						std::cerr << who.role() << " plays an illegal action!" << std::endl;
						const char* reason[] = {
							"legal",
							"illegal_turn",
							"illegal_pass",
							"illegal_out_of_range",
							"illegal_not_empty",
							"illegal_suicide",
							"illegal_take",
							"unknown",
						};
						std::cerr << "current state: " << std::endl << game.state();
						int code = move.apply(game.state());
						std::cerr << "action: " << args[1] << " " << args[2] << std::endl;
						std::cerr << "reason: " << reason[std::min(-code, 7)] << std::endl;
						break;
					}
				} else if (args[0] == "genmove") { // generate a move and play
					action::place move = who.take_action(game.state());
					if (game.apply_action(move) == true) {
						reply = move.position();
					} else { // I have no legal move to play
						reply = "resign";
					}
				}

			} else if (args[0] == "clear_board" || args[0] == "quit") { // reset game, or quit
				if (stats.is_episode_ongoing()) { // should close an opened episode
					agent& win = stats.back().last_turns(black, white);
					stats.close_episode(win.name());
					black.close_episode(win.name());
					white.close_episode(win.name());
				}
				if (args[0] == "quit") break; // quit GTP shell

			} else if (args[0] == "showboard") { // print the board
				std::stringstream buf;
				buf << (stats.is_episode_ongoing() ? stats.back().state() : board());
				reply = "\n" + buf.str();
				reply.pop_back(); // remove a new line

			} else if (args[0] == "boardsize") { // set the board size
				size_t size = std::stoul(args[1]);
				if (size != board::size_x || size != board::size_y) {
					std::cerr << "board size mismatch: " << args[1] << std::endl;
				}
				if (size > board::size_x || size > board::size_y) break;

			} else if (args[0] == "name") { // report the name of the program
				reply = name;
			} else if (args[0] == "version") { // report the version number of the program
				reply = version;
			} else if (args[0] == "protocol_version") { // report GTP protocol version
				reply = "2";
			} else if (args[0] == "list_commands") { // print supported commands
				reply = "play\n" "genmove\n" "clear_board\n" "showboard\n" "boardsize\n"
				        "name\n" "version\n" "protocol_version\n" "list_commands\n" "quit\n";
			} else {
				reply = "unknown command";
			}

			std::cout << "= " << reply << std::endl << std::endl;
		}
	}

	if (save_path.size()) {
		std::ofstream out(save_path, std::ios::out | std::ios::trunc);
		out << stats;
		out.close();
	}

	return 0;
}
