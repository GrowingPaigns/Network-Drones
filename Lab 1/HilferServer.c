#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/*********************************************************/
/* this is the server for lab0. it will receive a msg    */
/* from the client, and print it out. Make this your     */
/* own. Comment it, modularize it, learn it, fix it, etc */
/*********************************************************/

/* confirms valid num of arguments, as well as ip and port value/length */
void checkSrvVars(int argCount, char *argVec[], int pNum, int scDesc){
  int i; 
  
  /* first, decide if we have the right number of parameters */
  if (argCount < 2) {
    printf("[ERROR - Missing Variables: server <portnumber>]\n");
    exit(1);
  }

  /* Validate that the descriptor has been filled */
  if (scDesc == -1) {
    perror("[ERROR - Socket descriptor invalid]\n");
    exit(1);
  }
  if (scDesc == 0) {
    perror("[ERROR - Socket failed to connect]\n");
    exit(1);
  }

  /* Check Port Value + Range*/
  /* now fill in the address data structure we use to sendto the server */
  for (i = 0; i < strlen(argVec[1]); i++) {
    if (!isdigit(argVec[1][i])) {
      printf("[port ERR - The port number isn't a valid int value]\n");
      exit(1);
    }
  }
  if ((pNum > 65535) || (pNum < 0)) {
    printf("[port ERR - You entered an invalid port number (range: 0-65535)]\n");
    exit(1);
  }
  
}


/* checks if bind succeeded in attaching a local name to the sockDesc */
void checkBindRC(int scDesc, struct sockaddr_in srvAddr){
  int returnCode;  
   /* the next step is to bind to the address */
  returnCode = bind(scDesc, (struct sockaddr *)&srvAddr, sizeof(struct sockaddr));

  if (returnCode < 0) {
    perror("[Binding ERROR]");
    exit(1);
  }
}


/* checks if data was recieved successfully */
void checkSocketRC(int scDesc, char bffRcv[], struct sockaddr_in fromAddr, socklen_t fromSockLen){
  
  int flags = 0; // used for recvfrom
  int returnCode;

  // Gathers data one line at a time
  returnCode = recvfrom(scDesc, bffRcv, 1000, flags,
                (struct sockaddr *)&fromAddr, &fromSockLen);

  if (returnCode == 0) {
    /* If there is a "Success" listed after this error appears, 
       it means that the EOF was reached. This funct triggers because   
       checkSocketRC is called again, but has no more data to send */
    perror("[recvfrom ERROR - No bytes recieved]");
    exit(1);
  } 
  if (returnCode == -1) {
    perror("[recvfrom ERROR - Socket Issue]");
    exit(1);
  }  
  
  
  
}

//--------------------------------------------------------------

// Struct for storing our pairs
struct keyValuePairs{
  char key[200];
  char value[200];
};

// Reusable function to replace the character spaces in 'msg' 
void replaceSpace(char *str, char chReplace, char chQuote, char chSpaceToChange){

  int lineLength = strlen(str);
  int whiteSpaceCounter = 0;

  // Check for blank space until the end of line
  for (int i = 0; i < lineLength; i++){
    
    if (str[i] == chQuote){
      
      whiteSpaceCounter++;
    }
    // if a quotation was found, and the string char == chSpaceToChange,
    // replace with chReplace
    if (whiteSpaceCounter == 1){ 
      if (str[i] == chSpaceToChange){
        str[i] = chReplace;
      }
    }
    
  }
  
}



int main(int argc, char *argv[]) {
  int socketDesc;                  /* socket descriptor */
  struct sockaddr_in serverAddress;/* server gnode address */
  struct sockaddr_in fromAddress;  /* address of sender */
  char bufferReceived[1000];       // used in recvfrom
  int portNumber = 0;              // get this from command line
  socklen_t fromLength;
  
  
  socketDesc = socket(AF_INET, SOCK_DGRAM, 0); /* create a socket */
  portNumber = strtol(argv[1], NULL, 10); /* converts port to long int */
  
  // Ensures proper no. of vars, and no corrupt vars
  checkSrvVars(argc, argv, portNumber, socketDesc);

  
  serverAddress.sin_family = AF_INET;         /* use AF_INET family addresses */
  serverAddress.sin_port = htons(portNumber); /* convert provided port number */
  serverAddress.sin_addr.s_addr = INADDR_ANY; /* listen across all available addresses */

  checkBindRC(socketDesc, serverAddress);

  /* Specifies the length of the buffer being pointed to */
  fromLength = sizeof(struct sockaddr_in);
  memset(bufferReceived, 0, 1000); // Zero out the buffer
  
  int it = 0;
  int loopIt = 1;
  struct keyValuePairs kvp[200];
  // Makes the loop scaleable to different file sizes 
  while (it < fromLength){
    /* Checks if server is able to properly recieve data sent by client,
       gathers string data in buffer */  
    checkSocketRC(socketDesc, bufferReceived, fromAddress, fromLength);
  
    
    int noOfKVP = 0;

    // Replaces white space with '^'
    replaceSpace(bufferReceived, '^', '"', ' ');
    
    /* Defines what character separates different key:value pairs 
       (null returned when there are no more tokens) */
    char *nextKVP = strtok(bufferReceived, " ");
  
  
    while (nextKVP != NULL ){
      // Checks for specific character in the string that nextKVP iterates through
      char *kvIdentifier = strchr(nextKVP, ':'); 
      
    
      // If a ':' is identified....
      if (kvIdentifier != NULL){
        /* Copies the string of the key and stores it. Starts from the previous " " 
           space and ends on the kvIdentifier (:). Stores the str in the key struct 
           at the address determined by "noOfKVP" */
        strncpy(kvp[noOfKVP].key, nextKVP, kvIdentifier - nextKVP);
        strncpy(kvp[noOfKVP].value, kvIdentifier+1, strlen(nextKVP)-(kvIdentifier - nextKVP) - 1);
        
      }
      // Replaces '^' space with ' ' if inbetween " "
      replaceSpace(kvp[noOfKVP].value, ' ', '"', '^');
      
      
      nextKVP = strtok(NULL, " ");
      noOfKVP++;
    }
  
  
  
      /* Signals how many times 'checkSocketRC' 
         has been called/data has been collected */
      printf("\nRecieved %d key-value pairs on loop %i\n\n", noOfKVP, loopIt);
      for (int i = 0; i < noOfKVP; i++){
        // Outputs kvps with proper spacing
        printf("%-20s %-20s\n", kvp[i].key, kvp[i].value);
      }
    
    
    loopIt++;
    it++;
  }

} // end of main()