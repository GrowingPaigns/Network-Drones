## Communication System - Network Drones
The directories in this repository represent the first and final edition for my network programing homework assignments.
The included client/server code was refined with each lesson throughout the year and ultimately grew into the C file found in the Lab 8 directory

### 📂 [Lab 1 Folder](https://github.com/GrowingPaigns/Network-Drones/tree/main/Lab%201):
Contains the foundational client/server archetecture I created. 

The server establishes a communication between clients where in which a client can send a file full of key:value pairs, and have it be recieved by any other active clients. The file is sent line by line, and each time the same message is sent, the version of the message is iterated.

### 📂 [Lab 8 Folder](https://github.com/GrowingPaigns/Network-Drones/tree/main/Lab%208):
Contains the endpoint of the semester's worth of work

The server and client architecture has, by this point, been combined into a single "drone" file. The concept of grid based locations has been introduced. As seen in the program start command (./drone8 XXXXX 3 5), the program is initially started using a Port Number and by specifying the grid the drone will be operating in (columns, rows). Each port number has an attached IP address and Location specified in the config.file. If drones are 2 or less spaces away from one another in the grid (including diagonally) they will be able to communicate, else, they will print an `[OUT OF RANGE]` message.

Beyond that, each drone specified in the config, and started by a user, will be able to send and recieve messages sent on the network. However, each drone will only print out the actual message being sent if the drone's port matches the "toPort" variable in the sent message. Messages at this point are also manually typed in, the only details that need to be entered however are the 'msg' (`msg:"<text>"`) and 'toPort' (`toPort:<PortToSendTo>`) variables. Optionally you can also specify a move command (`move:<LocationToMoveTo>`), all other message variables are automatically handled.

When messages are received by a drone, an acknowledgement message (`type:ACK`) gets automatically returned to the sender by the receiving drone to signify that there was a successful delivery. Messages sent by each port are saved by the other drones even if the message is not destined for them. This is so that when a drone moves, the stored messages can be re-forwarded, which in turn allows the "drones" to carry messages to out of range recipients.

In addition, all messages have a Time to Live (TTL) which is how long/many times a message can be forwarded before it expires. When moving, all message times to live are reset, and when they reach 0 after re-sending them, they are deleted from the storage.

There are also numerous quality of life improvements that have been added, such as the forwarding drones checking sender/receiver ports so as to not resend messages to drones that have already seen them.
The version from assignment 1 has now changed to seqNumber, but functions the same way. That is, by iterating message numbers each time one is sent so as not to send duplicate messages.

### (To be added) 
Another folder will eventually be added as I refine, and implement more into the project
