# ------------------------------------------------
# Team: The Assignment Team
# Members: 
#    Gurbir Sandha - 1615292
#    Arjun Mehta - 1618364
# CMPUT 275, Winter 2021
# 
# Assignment: Trivial Navigation System - Part 2
# ------------------------------------------------
all: server client

server: server.o dijkstra.o digraph.o
	g++ server/server.o server/dijkstra.o server/digraph.o -o server/server

dijkstra.o: heap.h dijkstra.h dijkstra.cpp
	g++ server/dijkstra.cpp -o server/dijkstra.o -c

server.o: server.cpp wdigraph.h heap.h dijkstra.h digraph.h
	g++ server/server.cpp -o server/server.o -c

digraph.o: digraph.h digraph.cpp wdigraph.h
	g++ server/digraph.cpp -o server/digraph.o -c

client: client.o
	g++ client/client.o -o client/client

client.o: client.cpp
	g++ client/client.cpp -o client/client.o -c

clean:
	rm -f dijkstra.o digraph.o server.o server client client.o inpipe outpipe