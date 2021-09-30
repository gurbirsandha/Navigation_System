#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
// Add more libraries, macros, functions, and global variables if needed

#include <string.h>
#include <sstream>
#include <cstring>
#include <sys/socket.h>		// socket, connect
#include <arpa/inet.h>		// inet_aton, htonl, htons

#define BUFFER_SIZE 1024

using namespace std;

int create_and_open_fifo(const char * pname, int mode) {
    // creating a fifo special file in the current working directory
    // with read-write permissions for communication with the plotter
    // both proecsses must open the fifo before they can perform
    // read and write operations on it
    if (mkfifo(pname, 0666) == -1) {
        cout << "Unable to make a fifo. Ensure that this pipe does not exist already!" << endl;
        exit(-1);
    }

    // opening the fifo for read-only or write-only access
    // a file descriptor that refers to the open file description is
    // returned
    int fd = open(pname, mode);

    if (fd == -1) {
        cout << "Error: failed on opening named pipe." << endl;
        exit(-1);
    }

    return fd;
}

int main(int argc, char const *argv[]) {
    const char *inpipe = "inpipe";
    const char *outpipe = "outpipe";

    int in = create_and_open_fifo(inpipe, O_RDONLY);
    cout << "inpipe opened..." << endl;
    int out = create_and_open_fifo(outpipe, O_WRONLY);
    cout << "outpipe opened..." << endl;

    // Your code starts here

    // Here is what you need to do:
    // 1. Establish a connection with the server
    // 2. Read coordinates of start and end points from inpipe (blocks until they are selected)
    //    If 'Q' is read instead of the coordinates then go to Step 7
    // 3. Write to the socket
    // 4. Read coordinates of waypoints one at a time (blocks until server writes them)
    // 5. Write these coordinates to outpipe
    // 6. Go to Step 2
    // 7. Close the socket and pipes


	// Declare structure variables that store local and peer socket addresses
	// sockaddr_in is the address sturcture used for IPv4 
	// sockaddr is the protocol independent address structure

    int SERVER_PORT = stoi(argv[1]); // Port Number
    string SERVER_IP = argv[2]; // IP


	struct sockaddr_in my_addr, peer_addr; // create sockets

	// zero out the structor variable because it has an unused part
	memset(&my_addr, '\0', sizeof my_addr);

	// Declare socket descriptor
	int socket_desc;

	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_desc == -1) {
		std::cerr << "Listening socket creation failed!\n";
		return 1;
	}
	cout << "Socket Created" << endl;
	// Prepare sockaddr_in structure variable
	peer_addr.sin_family = AF_INET;							
	peer_addr.sin_port = htons(SERVER_PORT);				
	peer_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (connect(socket_desc, (struct sockaddr *) &peer_addr, sizeof peer_addr) == -1) {
		std::cerr << "Cannot connect to the host!\n";
		close(socket_desc);
		return 1;
	}
	std::cout << "Connection established with " << inet_ntoa(peer_addr.sin_addr) << ":" << ntohs(peer_addr.sin_port) << "\n";

	// create N, inbound, and buffer char arrays for buffers (use dependa)
	char Nbuffer[BUFFER_SIZE] = {};
	char inbound[BUFFER_SIZE] = {};
    char buffer[BUFFER_SIZE];

    // acknowledgement character
    char ack[] = {'A'};

    // timer and option set for 1 second timeout
    struct timeval timer = {.tv_sec = 1, .tv_usec = 0};

    if(setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (void *) &timer, sizeof(timer)) == -1){
      close(socket_desc);
      return 1;
    }
    
    cout << "Reader started..." << endl;
    while (true) { // loop until plotter closed
    	// declare exit condition flags
    	bool timeout_flag = false;
    	bool while_flag = true;


    	// soft reset buffers
    	inbound[BUFFER_SIZE] = {};
    	Nbuffer[BUFFER_SIZE] = {};
    	buffer[BUFFER_SIZE] = {};

    	string temp = "";

    	// read from pipe
	    int bytesread = read(in, buffer, BUFFER_SIZE);


	  	if(strncmp("Q", buffer, 1) == 0){ // exit on Q from pipe
	  		send(socket_desc, buffer, sizeof buffer, 0);
	  		break;

	  	}

  		// initialize start and end points
  		long long start_lat = 0;
  		long long start_lon = 0;
  		long long end_lat = 0;
  		long long end_lon = 0;


  		// create a strtok to parse incoming char array
        char * ptr1 = strtok(buffer, " \n");
        start_lat = static_cast<long long>(stod(ptr1)*100000);

        ptr1 = strtok(NULL, " \n");
        start_lon = static_cast<long long>(stod(ptr1)*100000);

        ptr1 = strtok(NULL, " \n");
        end_lat = static_cast<long long>(stod(ptr1)*100000);

        ptr1 = strtok(NULL, " \n");
        end_lon = static_cast<long long>(stod(ptr1)*100000);

        temp = "R " + to_string(start_lat) + " " + to_string(start_lon) +
        " " + to_string(end_lat) + " " + to_string(end_lon); // save message to temp string


        string Rboy = temp; // copy temp to R specific string

 		while(while_flag){ // loop until current coord plotted
	    	send(socket_desc, Rboy.c_str(), strlen(Rboy.c_str()) + 1, 0); // SEND R MESSAGE

	    	Nbuffer[BUFFER_SIZE] = {}; // soft reset

	    	for(int i = 0; i < 1024; i++){ // hard reset (required)
	    		Nbuffer[i] = NULL;
	    	}

	    	int rec_size = recv(socket_desc, Nbuffer, BUFFER_SIZE, 0); // N READ IN

	        if (rec_size == -1) {
	            std::cout << "Timeout occurred. Waiting for N...\n";
	            timeout_flag = true;

	            continue; // jump back to start of while_flag loop (resend R)
	            }

	         // parse N message into 'N' and integer
	    	char * ptr2 = strtok(Nbuffer, " ");
	    	ptr2 = strtok(NULL, " ");
	    	int N = stoi(ptr2);

	    	ptr2 = NULL; // reset

	    	temp = ""; // reset

	    	for(int i = 0; i < N; i++){
	    		timeout_flag = false; // reset timeout flag
	    		for(int i = 0; i < 1024; i++){ // hard inbound buffer reset
	    			inbound[i] = NULL;
	    		}
	    		send(socket_desc, ack, 1, 0); // send acknowledgement

	    		rec_size = recv(socket_desc, inbound, BUFFER_SIZE, 0); // WAYPOINTS
	        	if (rec_size == -1) {
	            	std::cout << "Timeout occurred. Wait for Waypoint\n";
	            	timeout_flag = true;
	            	break; // exit waypoint for loop, jump to rerun nested while
	            }

	    		for(int j = 2; j < strlen(inbound); j++){ // append non 'W ' chars to temp string
	    			temp += inbound[j];
	    		}
	    	}

	    	if(timeout_flag){ // if flag active, reset to send R
	    		timeout_flag = false;
	    		continue;
	    	}

	    	inbound[BUFFER_SIZE] = {}; // soft reset

	    	if(N > 1) {
	    		// write temp string to outpipe, remove cast to remove final end char
	    		char outwrite[temp.length()] = {};
	    		strcpy(outwrite, temp.c_str());
	    		write(out, outwrite, sizeof outwrite);


	    		// DEBUG: check what is written
	    		// cout << outwrite;
		
	    		send(socket_desc, ack, 1, 0); // acknowledgement sent to server

	    		rec_size = recv(socket_desc, inbound, BUFFER_SIZE, 0); // E RECEIVE
	        	if (rec_size == -1) {
	            	std::cout << "Timeout occurred. Wait for E\n";
	            	timeout_flag = true;
	            	continue; // reset to send R, start of nested while loop
	            }

	    		if(strncmp(inbound, "E", 1) == 0){ // E CHECK
		    		// if E inbound, create proper E string, format and write to pipe
			      	temp = "E\n";
			    	char E[temp.length()] = {}; //size down by 1
			    	strcpy(E, temp.c_str());
			    	write(out, E, sizeof E);
	    		}
	    	}
	    	// N is 1 case, receive E slight difference
	    	else if(N==1){
	    		send(socket_desc, ack, 1, 0); // send acknowledgement
	    		rec_size = recv(socket_desc, inbound, BUFFER_SIZE, 0); // E, TIMEOUT

	        	if (rec_size == -1) {
	            	std::cout << "Timeout occurred. Wait for E\n";
	            	timeout_flag = true;
	            	continue; // reset to send R, start of nested while loop
	            }

	    		if(strncmp(inbound, "E", 1) == 0){
			      	temp = "E\n";
			    	char E[temp.length()] = {};
			    	strcpy(E, temp.c_str());
			    	write(out, E, sizeof E);
	    		}
	    	}
	    	else {
		      	temp = "E\n"; // 0 case, ensure points are plotted
		    	char E[temp.length()] = {};
		    	strcpy(E, temp.c_str());
		    	write(out, E, sizeof E);		
	    	}

	    	for(int i = 0; i < 1024; i++){ // reset inbound char array
	    		inbound[i] = NULL;
	    	}
	    	/*
			* exit inner while loop if end is reached
			* This means new R coordinates are going to be sent as opposed
			* to resend old ones in event of packet drop
			*/
	    	while_flag = false;

 		}

    }

    // // close socket
    close(socket_desc);

    // Your code ends here

    close(in);
    close(out);
    unlink(inpipe);
    unlink(outpipe);
    return 0;
}