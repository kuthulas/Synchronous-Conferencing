POSIXSyncConferencing
=====================

POSIX-based synchronous text conferencing

ECEN602 HW1 Programming Assignment
----------------------------------

Team Number: 31
Member 1 # Thulasiraman, Kumaran (UIN: 223003944)
Member 2 # Manjunath, Jnanesh (UIN: 322005490)

---------------------------------------

Description/Comments:
--------------------
1. Client enters JOIN to enter an ongoing chat
2. Same username will be rejected by server 
3. User exits chat session with Ctrl+C

Unix command for starting server:
------------------------------------------
./server SERVER_IP SERVER_PORT MAX_CLIENTS

Unix command for starting client:
------------------------------------------
./client USERNAME SERVER_IP SERVER_PORT

Design
-------
Both server and client are designed in a modular manner.
The list of modules used are as below:
Module to handle initial join – handshake
Module to populate fields in the msg as per given case - dispatch
Module to send out messages to intended audience – msgplex
Module to separate initial socket setup – nexus
Module to handle incoming msg from server(Client only) -  patchback
