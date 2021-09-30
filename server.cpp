#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>
#include <list>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "wdigraph.h"
#include "dijkstra.h"

#define LISTEN_BACKLOG 50
#define BUFFER_SIZE 1024




struct Point {
    long long lat, lon;
};

// returns the manhattan distance between two points
long long manhattan(const Point& pt1, const Point& pt2) {
  long long dLat = pt1.lat - pt2.lat, dLon = pt1.lon - pt2.lon;
  return abs(dLat) + abs(dLon);
}

// finds the point that is closest to a given point, pt
int findClosest(const Point& pt, const unordered_map<int, Point>& points) {
  pair<int, Point> best = *points.begin();
  for (const auto& check : points) {
    if (manhattan(pt, check.second) < manhattan(pt, best.second)) {
      best = check;
    }
  }
  return best.first;
}

// reads graph description from the input file and builts a graph instance
void readGraph(const string& filename, WDigraph& g, unordered_map<int, Point>& points) {
  ifstream fin(filename);
  string line;

  while (getline(fin, line)) {
    // split the string around the commas, there will be 4 substrings either way
    string p[4];
    int at = 0;
    for (auto c : line) {
      if (c == ',') {
        // starting a new string
        ++at;
      }
      else {
        // appending a character to the string we are building
        p[at] += c;
      }
    }

    if (at != 3) {
      // empty line
      break;
    }

    if (p[0] == "V") {
      // adding a new vertex
      int id = stoi(p[1]);
      assert(id == stoll(p[1])); // sanity check: asserts if some id is not 32-bit
      points[id].lat = static_cast<long long>(stod(p[2])*100000);
      points[id].lon = static_cast<long long>(stod(p[3])*100000);
      g.addVertex(id);
    }
    else {
      // adding a new directed edge
      int u = stoi(p[1]), v = stoi(p[2]);
      g.addEdge(u, v, manhattan(points[u], points[v]));
    }
  }
}

// Keep in mind that in Part I, your program must handle 1 request
// but in Part 2 you must serve the next request as soon as you are
// done handling the previous one
int main(int argc, char* argv[]) {

  int PORT = stoi(argv[1]);

  WDigraph graph;
  unordered_map<int, Point> points;
  // build the graph
  readGraph("edmonton-roads-2.0.1.txt", graph, points);

  // In Part 2, client and server communicate using a pair of sockets
  // modify the part below so that the route request is read from a socket
  // (instead of stdin) and the route information is written to a socket

  // create sockets
  struct sockaddr_in my_addr, peer_addr;

  // zero out the structor variable because it has an unused part
  memset(&my_addr, '\0', sizeof my_addr);

  // declare variables for socket descriptors 
  int lstn_socket_desc, conn_socket_desc;

  // char array buffer for reception from client
  char start_buffer[BUFFER_SIZE] = {};


  // create listening socket
  lstn_socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  if (lstn_socket_desc == -1) {
    std::cerr << "Listening socket creation failed!\n";
    return 1;
  }

  // prepare sockaddr_in structure variable
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(PORT);
  my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  // note bind takes in a protocol independent address structure
  // hence we need to cast sockaddr_in* to sockaddr*
  if (bind(lstn_socket_desc, (struct sockaddr *) &my_addr, sizeof my_addr) == -1) {
      std::cerr << "Binding failed!\n";
      close(lstn_socket_desc);
      return 1;
  }

  std::cout << "Binding was successful\n";

  // check if listenable
  if (listen(lstn_socket_desc, LISTEN_BACKLOG) == -1) {
    std::cerr << "Cannot listen to the specified socket!\n";
      close(lstn_socket_desc);
      return 1;
  }

  socklen_t peer_addr_size = sizeof my_addr;

  while (true) {
    bool timeout_flag = false;

    //create connection socket
    conn_socket_desc = accept(lstn_socket_desc, (struct sockaddr *) &peer_addr, &peer_addr_size);
    if (conn_socket_desc == -1){
      std::cerr << "Connection socket creation failed!\n";
      return 1;
    }

    std::cout << "Connection request accepted from " << inet_ntoa(peer_addr.sin_addr) << ":" << ntohs(peer_addr.sin_port) << "\n";

    // create a timer and set the socket option to timeout every second
    struct timeval timer = {.tv_sec = 1, .tv_usec = 0};

    if(setsockopt(conn_socket_desc, SOL_SOCKET, SO_RCVTIMEO, (void *) &timer, sizeof(timer)) == -1){
      close(conn_socket_desc);
      return 1;
    }


      while (true) {
        string str = ""; // initalize temp writing string
        timeout_flag = false; // set timeout flag false, if true reset to start of this loop
        
        // hard reset start buffer
        for(int i = 0; i < 1024; i ++){
          start_buffer[i] = NULL;
        }
        struct Point sPoint, ePoint; // create point objects


        int rec_size = recv(conn_socket_desc, start_buffer, BUFFER_SIZE, 0); // READ R message
        if(rec_size == -1){
          timeout_flag = true;
          continue; // retart loop, will continually timeout while waiting (feature)
        }

        if(strncmp("Q", start_buffer, 1) == 0){ // exit code check
          timeout_flag = true;
          break;
        }

        // initailze / reset points to 0 (avoid wacky behav.)
        sPoint.lat = 0;
        sPoint.lon = 0;

        ePoint.lat = 0;
        ePoint.lon = 0;

        // tokenize input R string
        char * ptr1 = strtok(start_buffer, " \n"); // R char

        ptr1 = strtok(NULL, " \n");
        sPoint.lat = stoll(ptr1);

        ptr1 = strtok(NULL, " \n");
        sPoint.lon = stoll(ptr1);

        ptr1 = strtok(NULL, " \n");
        ePoint.lat = stoll(ptr1);

        ptr1 = strtok(NULL, " \n");
        ePoint.lon = stoll(ptr1);


        int start = findClosest(sPoint, points), end = findClosest(ePoint, points);
        // run dijkstra's, this is the unoptimized version that does not stop
        // when the end is reached but it is still fast enough

        // start dijkstra
        unordered_map<int, PIL> tree;
        dijkstra(graph, start, tree);
        str = ""; // reset string

        // no path
        if (tree.find(end) == tree.end()) {
            cout << "N 0" << endl;
            // send N 0 string without end char
            str = "N 0" ;
            send(conn_socket_desc, str.c_str(), (sizeof str.c_str())-1, 0); 
        }
        else {
          // read off the path by stepping back through the search tree
          list<int> path;
          while (end != start) {
            path.push_front(end);
            end = tree[end].first;
          }
          path.push_front(start);

          // output the path terminal
          cout << "N " << path.size() << endl;
          // create and send 'N n' string, chop end char

          str = "N " + to_string(path.size());
          send(conn_socket_desc, str.c_str(), (sizeof str.c_str())-1, 0);
          
          for (int v : path) {

            rec_size = recv(conn_socket_desc, start_buffer, BUFFER_SIZE, 0); // TIMEOUT 
            if (rec_size == -1) {
              std::cout << "Timeout occurred. Waiting for A...\n";
              timeout_flag = true;
              break; // break out of for loop, send to while loop reset
            }

            if(strncmp("A", start_buffer,1) == 0){ 
              // send correctly formatted  W coordinate string
              // simpler to handle to double on server side
              str = "W " + to_string(double(points[v].lat)/100000.0) + " " +
              to_string(double(points[v].lon)/100000.0) + '\n';


              // create char array minus end char, send formatted
              char cpath[str.length()] = {};
              strcpy(cpath, str.c_str());
              send(conn_socket_desc, cpath, sizeof cpath, 0);

              // terminal output:
              cout << "W " << points[v].lat << ' ' << points[v].lon << endl;
            }
            else { // else cond: something went wrong (most likely timeout)
              timeout_flag = true;
              continue;
            }

            
          }
          if(timeout_flag) { // while loop reset relay

            for(int i = 0; i < 1024; i++){ // hard reset buffer state
              start_buffer[i] = NULL;
            }
            continue;
          }

          rec_size = recv(conn_socket_desc, start_buffer, BUFFER_SIZE, 0); // TIMEOUT
          if (rec_size == -1) {
            std::cout << "Timeout occurred. Wait for A before E\n"; // specify where A not
            timeout_flag = true;
            continue; // reset state
          }


          if(strncmp("A", start_buffer,1) == 0){  // check for ack. char
            // init E char minus 
            str = "E";
            char E[str.length()] = {};
            strcpy(E, str.c_str());
            send(conn_socket_desc, E, sizeof E, 0);

            // terminal check
            cout << "E" << endl;
          }

          else { // (most likely timeout)
            timeout_flag = true;
            continue;
          }
        }
      }


      if(timeout_flag){ // reset relay (break out of inner while)
        for(int i = 0; i < 1024; i++){ // reset buffer
          start_buffer[i] = NULL;
        }
        break; // start shutdown closing
      }
  }

  // close socket descriptors
  close(lstn_socket_desc);
  close(conn_socket_desc);

  return 0;
}



