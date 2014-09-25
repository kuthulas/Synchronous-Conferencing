ECEN602 HW1 Programming Assignment
----------------------------------

Team Number: 31
Member 1 # Thulasiraman, Kumaran (UIN: 223003944)
Member 2 # Manjunath, Jnanesh (UIN: 322005490)
---------------------------------------

Description:
	This code-set written in C implements client and server procedures for SBCP-based synchronous text conferencing service. 
	All the message types defined by the SBCP have been implemented - JOIN, ACK, NAK, ONLINE, OFFLINE, SEND, FWD and IDLE.

Source files: 
	SBCP.h, client.c, server.c

Architecture:
	The server handles requests from  multiple clients through synchronous I/O multiplexing. Errors are handled at every stage of the session. 
	The client program is multithreaded. A separate thread is dedicated for detecting the idle state of the client, and reporting to server.
			The main thread handles the session logic, IO multiplexing and error handling.

Features:
	1. Server is initiated with a maxClients parameter; a single chatroom is in service.
	2. Client connects, and sends a JOIN request with its username. Client cannot send any other message before JOINing the session.
	3. If username already exists, or if maxClients is reached, server sends a NAK with a reason code. Else server accepts the request sending an ACK,
		and multicasts an ONLINE notification, for the server, to all the other connected clients.
	4. The accepted client gets the number and list of members already in the session.
	5. Client can now SEND a message. Server FWDs the message to all the other connected & joined clients.
	6. IDLE message is sent by the client to the server if the user has not typed anything for more than 10 seconds. The server then forwards the IDLE notification to others.
	7. Client can receive ONLINE, OFFLINE and IDLE notifications for other clients in the session.
	8. Client can disconnect abruptly. The server closes and cleans up the connection, freeing up the disconnected client's username.
	9. Any unexpected message is ignored and discarded by both the server and the client.
	
Usage: 

	Unix command for starting server:
	------------------------------------------
	./server SERVER_IP SERVER_PORT MAX_CLIENTS

	Unix command for starting client:
	------------------------------------------
	./client USERNAME SERVER_IP SERVER_PORT