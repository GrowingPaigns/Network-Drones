#### CSCI 3762 - Network Programming 
#### Samuel Hilfer
#### Due: 02/01/2023

## Client-Server Introduction (Lab 1)
 
|To-Do:							    | Completed?|
|----------------------------------------------------------------|---------|
| Client reads and sends txt doc one line at a time 	                   |√|
| Server recieves Client DGRAM				                                   |√|
| Server replaces 'msg' value with a readable string	                   |√|
| Server separates k:v pairs, stores them in a struct	                   |√|
| Server prints k:v pairs as expected			                               |√|

### The Beginnings of a New Project ...

Starting with the client, after establishing connection to the server I probe the user to input the name of a text document. This file is opened and read before the client then sends the document line-by-line to the server using 'getline' and 'sendto' functions. 

With the server, a lot more has changed. Before 'main', I created a structure to hold the different k:v pairs, and a reusable function which replaces the white space between two quotation marks with '^' characters. This function enables the program to properly read, store, and output the 'msg' key while still including '"' marks. 

Within 'main', our first new addition is a while loop which continually gathers data from the buffer (one line at a time) until the EOF is reached. In this loop, the white space replacement function is called, and I also define what different characters within the text doc mean. 

I use the 'strtok' function to define a token which signals where different k:v pairs begin (i.e. white space signifies the end of one k:v pair and the start of a new k:v pair). From there I define the character to look for which separates the keys from values (i.e. ':'). If a ':' is detected I use 'strncpy' to copy the keys and values into their separate struct variables. Once the keys and values are stored, I again check for '"' marks, and replace all '^' characters inbetween those '"' marks with white space. 

Finally, for each time that I receive data I output those variables with spacing similar to the example shown in our lab instructions. If there is still data that can be read from the buffer, this process loops, and once EOF is reached, the program terminates. 

Besides these primary additions, there have been more comments added, as well as a few more specific error checks for some of the new additions made.

------------------------------------------------------------------------------------------

### Notes for runtime: 

When running the program, you will notice that the two lines present in the text doc are formatted and displayed in two different loops. With the time I had available to work on this assignmnt, I wasn't able to get the output to display as one long list. Although, this problem can be remedied relatively easily if necessary for future assignments by simply collecting all the data from the buffer before I start any further opperations on it. 

Another thing to note, after the data has been printed, the server will output an error message. This message is intentional, and is how the program terminates itself after exhausting the data stored by the buffer. The message states, "[recvfrom ERROR - No bytes recieved]: Success". As long as "Success" appears after the error message, the system is functioning properly. The reason this error pops up is because my 'checkSocketRC' function is trying to assign the recieved data to the buffer, but since the program has reached the EOF, there is no more data to assign, thus, no bytes get recieved. Thereby, in this instance, the error is more of a success check than anything. 

	==========================================================================================
						
			      Below is the functionality of Cli/Serv from Lab 0.
		    There have been no updates to this information from my Lab 0 README,
	      it is only here as a refresher of what the rest of the cli/server code still does

	==========================================================================================


#### In terms of the Client Program...

The function 'checkCliVars' firstly ensures that the argument count from our main function's 'argc' paramater doesn't equal or exceed 3. From there, it then checks if the socket descriptor was able to gather the different variables it requires, and if not, there are two error handling cases which detail if the descriptor recieved broken data, or if the socket simply failed to connect. To end the function, I moved the checks for both IP and Port address into this function, and specified between a few different IP error values.

The function 'checkSocketRC' verifies that the client was able to successfully send its data to the server through the use of sendto()'s return code. This code is referenced to make sure that variables are not too long for the buffer, whle also being checked for send errors.
	

As for the 'main' function of the Client program, we start by setting up the socket service before assigning the IP and port values of argv to different variables. We then specify the address family and convert the user's input regarding port and IP we null out our buffer and assign a user defined file to it that will be sent to the server. 

------------------------------------------------------------------------------------------
#### In terms of the Server Program...

I created three void functions in this program (checkBindRC; checkSocketRC; checkSrvVars). Past that, I changed variable names and added some reduced comments.

The function 'checkBindRC' verifies whether or not the bind function successfully attached an identifier to our socket, and if it has collcted an address it can point to. If it doesn't succeed, the error is caught through the use of a return code, and is displayed to the user.

The function 'checkSocketRC' verifies that the data sent to it has been properly recieved. This is done through the use of gathering the return code of the function 'recvfrom()'. I generated an if statement for both return codes, specifying between socket and byte issues.

Finally, just like in the client program, the function 'checkSrvVars' verifies the integrity of each of the required variables (argument count, socket description, port, IP)


The 'main' function of the Server starts by creating a description of the socket and gathering the port value before we then call the check for our variables. After that we specify the address family and convert the port, and then tell the server to listen across all available addresses by using 'INADDR_ANY'. Then we specify the length of the file we are receiving, verify our bind function, and clear the buffer which will hold the received data. Finally, we check our socket return codes before printing out the data we were sent.






