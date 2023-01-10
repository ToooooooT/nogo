all:
	g++ -std=c++11 -pthread -O3 -g -Wall -fmessage-length=0 -o nogo_ nogo.cpp
black:
	./nogo_ --total 1 --name="tooot" --black="search=MCTS thread=4" --white="search=Random"
white:
	./nogo_ --total 1 --name="tooot" --black="search=Random" --white="search=MCTS thread=4"
test:
	./nogo --total=1 --black="search=MCTS simulation=1000" --white="search=Random"
clean:
	rm nogo
