all:
	g++ -std=c++11 -O3 -g -Wall -fmessage-length=0 -o nogo nogo.cpp
black:
	./nogo --total=1000 --black="search=MCTS simulation=1000" --white="search=Random"
clean:
	rm nogo
