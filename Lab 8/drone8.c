/*
 * Title: CSCI 3762 - Lab 8
 * Author: Samuel Hilfer
 * Due Date: 01/03/23
 */

/* -- Completion notes --
 * Lab 8 functionality completed, multiple bugs still exist
 * [Main Bug] after sending a msg, changing location, and having that msg ttl
 * count down to 0, there  * is an error when trying to send an additional
 * buffer from the same machine
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define TTL 6
#define MAXSRV 100

// Struct for storing our parsed config
typedef struct srvAddresses {
  char ip[20];
  int port;
  int noOfSrv;
  char location[3];
  int seqNumber;
} srv_addr;

// Struct for storing our parsed pairs
typedef struct keyValuePairs {
  char key[200];
  char value[1000];
  char timeToLive[10];
} kValPairs;

// Struct for storing port/sequence vars for each address
// can probably be moved into srvAddresses in the future
typedef struct portSequence {
  int port;
  int seqNumber;
  int lastSender;

  int ackPort;
  int ackSeqNumber;

  bool seqIncreased;
} port_seq;

// Holds and returns the values for certain variables so that their values can
// be used across multiple functions by implementing pointers
typedef struct variableHolder {

  bool ttlExpired;
  bool ack;
  bool portPrevRecv;
  bool moveRecv;

  int timeTL;
  int ttlPrevious;
  int seqNum;
  int prevSeqNum;
  int permSeqHolder;
  int moveLocation;

  int kvpNo;
  int portHldr;
  int savePort[50];
  int distance;

  char sendPath[100];
  char saveBuffer[200];
  char bufferLoc[50];

} var_holder;

typedef struct bufferHolder {
  char buffer[200];
} resendBuffer;

// Holds string responses from the parsing section of inputParser
// used to eliminate unnecessary text if distance is too great
typedef struct stringHolder {
  char port[50];
  char version[50];
  char location[50];
  char myLocation[50];
  char ttl[50];
} string;

/* [Function Notes]
 * This function reads the lines from the config file and parses the values
 * into three accessable struct variables
 */
void parseConfig(char *fileName, srv_addr *srvAddr, int maxServers) {
  int srvAddrCount = 0; // iterator that tracks the number of servers in config
  char configBuffer[200];                 // buffer to store config
  FILE *configPTR = fopen(fileName, "r"); // read config into ptr

  // ensures config was able to be opened
  if (!configPTR) {
    perror("[ERROR - problem encountered while opening config]");
    exit(1);
  }

  // writes config file to buffer and ensures we are within server capacity
  while (fgets(configBuffer, 200, configPTR) != NULL &&
         srvAddrCount < maxServers) {
    //  reads from buffer into assigned struct locations
    if (sscanf(configBuffer, "%s %d %s", srvAddr[srvAddrCount].ip,
               &srvAddr[srvAddrCount].port,
               srvAddr[srvAddrCount].location) == 3) {
      //  when argumnt count reaches 3, increase the number of listed servers
      ++srvAddrCount;
    }
  }
  // if no servers are found, break
  if (srvAddrCount < 0) {
    printf("[ERROR - no servers available]\n");
    exit(1);
  }
  // returns the number of potential servers
  srvAddr->noOfSrv = srvAddrCount;
  fclose(configPTR);
}

/* [Function Notes]
 * Sends buffer to specified address
 * Prevents sending to addresses already received
 */
void cliSend(int numSrv, srv_addr srv[], int socketDesc, char userBuffer[],
             struct sockaddr_in serverAddress, var_holder *holder,
             int portNumber) {

  int portCount = 0;

  // double checks that there are available servers
  if (numSrv < 1) {
    printf("[ERROR - no servers available to send to]\n");
    exit(1);
  }

  serverAddress.sin_family = AF_INET; // Assign family

  // make a copy of the send path before modifying it
  char sendPathCopy[200];
  strcpy(sendPathCopy, holder->sendPath);

  // split the copied send path into individual ports
  char *nextPort = strtok(sendPathCopy, ",");
  while (nextPort != NULL && portCount < MAXSRV) {

    holder->savePort[portCount] = atoi(nextPort);
    nextPort = strtok(NULL, ",");
    portCount++;
  }

  // For all the addresses in config send a message
  for (int i = 0; i < numSrv; i++) {

    bool skipSending = false;

    // Check for server ports in send path
    for (int j = 0; j < portCount; j++) {
      if (srv[i].port == holder->savePort[j] || srv[i].port == portNumber) {
        skipSending = true;
        break;
      }
    }

    // If the server port is in the send path, skip sending
    if (skipSending) {
      if (srv[i].port == portNumber) {
        continue;
      }
      printf("|[will not resend to port: %d]\n", srv[i].port);
      continue;
    } else {
      holder->portHldr = srv[i].port;
    }

    // Specify port send to
    serverAddress.sin_port = htons(srv[i].port);
    // Specify ip to send to
    serverAddress.sin_addr.s_addr = inet_addr(srv[i].ip);

    // Sends data to above address if there is no error with sendto
    if (sendto(socketDesc, userBuffer, strlen(userBuffer), 0,
               (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
      perror("[ERROR - sendto issue]");
      exit(1);
    } else {
      continue;
    }
  }
}

/* [Function Notes]
 * Reusable function to replace the character spaces in 'msg' kvp
 */
void replaceSpace(char *lineIn, char charReplace, char charQuote,
                  char charSpaceToChange) {

  int lineLength = strlen(lineIn); // gets the length of the kvp value passed in
  int quoteCounter = 0;            // keeps track of where whitespace is found

  // Check for quotation marks within the received line
  for (int i = 0; i < lineLength; i++) {
    if (lineIn[i] == charQuote) {
      quoteCounter++; // if quotation mark is found...
    }

    // if a quotation mark was previously found, and the current character in a
    // string = charSpaceToChange, replace the space with charReplace
    if (quoteCounter == 1) {
      if (lineIn[i] == charSpaceToChange) {
        lineIn[i] = charReplace;
      }
    }
  }
}

/* [Function Notes]
 * Confirms valid number of arguments were entered. Confirms validity of ip,
 * and port value/length
 */
void checkArgs(int argCount, char *argVec[], int portNum, int scDesc) {
  int i;

  // first, decide if we have the right number of parameters
  if (argCount < 4) {

    printf("[ERROR - Potentially Missing Variables]\n"
           "[Check: <portNumber>, <rowSize>, <columnSize>]\n");
    exit(1);
  }

  // Validate that the descriptor has been filled
  if (scDesc == -1) {
    perror("[ERROR - Socket descriptor invalid]\n");
    exit(1);
  }
  if (scDesc == 0) {
    perror("[ERROR - Socket failed to connect]\n");
    exit(1);
  }

  // Check Port Value
  for (i = 0; i < strlen(argVec[1]); i++) {
    if (!isdigit(argVec[1][i])) {
      printf("[ERROR - provided port number isn't a valid integer value]\n");
      exit(1);
    }
  }
  // Check Port Range
  if ((portNum > 65535) || (portNum < 0)) {
    printf("[ERROR - you entered an invalid port number (range: 0-65535)]\n");
    exit(1);
  }

  // Check if proper grid values have been entered
  if (strlen(argVec[2]) < 1 && strlen(argVec[3]) < 1) {
    printf("[ERROR - invalid row and column arg size]");
  }
  if (strlen(argVec[2]) < 1) {
    printf("[ERROR - invalid row arg size]");
  }
  if (strlen(argVec[3]) < 1) {
    printf("[ERROR - invalid column arg size]");
  }
}

/* [Function Notes]
 * Checks if bind succeeded in attaching a local name to the sockDesc
 */
void srvBindRC(int scDesc, struct sockaddr_in srvAddr) {
  int returnCode;

  // binds the address that client functions connect to
  returnCode =
      bind(scDesc, (struct sockaddr *)&srvAddr, sizeof(struct sockaddr));

  if (returnCode < 0) {
    perror("[Binding ERROR]");
    exit(1);
  }
}

/* [Function Notes]
 * Ensures buffer is empty before checking if data was recieved successfully
 */
char *srvReceive(int scDesc, char bffRcv[], struct sockaddr_in fromAddr,
                 socklen_t fromSockLen) {

  int flags = 0; // determined by socket, influences the behavior of a funct
                 // beyond required args
  int returnCode;
  memset(bffRcv, 0, 1000); // clear the buffer
  // Gathers data one line at a time
  returnCode = recvfrom(scDesc, bffRcv, 1000, flags,
                        (struct sockaddr *)&fromAddr, &fromSockLen);

  if (returnCode == 0) {
    // If there is a "Success" listed after this error specifically appears, it
    // means that the EOF was reached. This triggers because checkSocketRC is
    // called again, but has no more data to send - (Potentially fixed with lab
    // 3 work)
    perror("|[recvfrom ERROR - No bytes recieved]");
    exit(1);
  }
  if (returnCode == -1) {
    perror("|[recvfrom ERROR - Socket Issue]");
    exit(1);
  }
  return bffRcv;
}

/* [Function Notes]
 * initial set up for drone address/arg checking/rg checkingsocket binding in
 * main()
 */
void startUpDrone(int *socketDesc, int *portNumber, int argc, char *argv[],
                  struct sockaddr_in serverAddress, const int noSRV,
                  srv_addr srv[], char *loc, fd_set socketFDS) {
  *socketDesc = socket(AF_INET, SOCK_DGRAM, 0); // create a socket
  *portNumber = strtol(argv[1], NULL, 10);      // converts port to long int

  /* ---- Setting Up Drone to Receive ---- */
  // Ensures proper num of vars, and no corrupt vars
  checkArgs(argc, argv, *portNumber, *socketDesc);
  serverAddress.sin_family = AF_INET;          // use AF_INET family addresses
  serverAddress.sin_port = htons(*portNumber); // convert provided port number
  serverAddress.sin_addr.s_addr =
      INADDR_ANY; // listen across all available addresses

  // gives a name to our descriptor
  srvBindRC(*socketDesc, serverAddress);
  if (socketDesc < 0) {
    perror("[ERROR - socket issue]");
  }

  // get location of drone for the initial display message
  for (int j = 0; j < noSRV; j++) {
    if (*portNumber == srv[j].port) {
      loc = srv[j].location;
    }
  }

  printf("\n[Welcome to Samuel Hilfer's Drone Program]\n[You are listening "
         "on Port %i, at location %s]\n",
         *portNumber, loc);

  FD_ZERO(&socketFDS);
}
/* [Function Notes]
 * Checks incomming ACK messages for port/seq numbers before storing the
 * related values for each drone in a struct
 */
void ackPortSequence(bool portCheck, int distance, int noOfKVP, kValPairs kvp[],
                     port_seq pSeq[], var_holder *holder) {

  int allNull = 0;
  int saveIndex = 0;
  int portVal = 0;

  // if it is meant for the machine
  if (portCheck && (distance <= 2 && distance > 0)) {
    for (int i = 0; i < noOfKVP; i++) {

      if (strcmp(kvp[i].key, "fromPort") == 0) {
        for (int j = 0; j < 200; j++) {
          // iterate through to check if port was previously seen
          if (pSeq[j].ackPort == atoi(kvp[i].value)) {
            allNull = 1;
            saveIndex = j; // if port was seen before, save the index
            portVal = atoi(kvp[i].value); // save the port
            break;
          }
          if (pSeq[j].ackPort == 0) {
            // if port has not been received previously
            allNull = 0;
          }
        }

        if (allNull == 0) {
          holder->portPrevRecv = false;
          // ensure space is empty before saving new port
          for (int j = 0; j < 200; j++) {
            if (!pSeq[j].ackPort && !pSeq[j].ackSeqNumber) {
              pSeq[j].ackPort = atoi(kvp[i].value);
              portVal = atoi(kvp[i].value);
              saveIndex = j;
              break;
            }
          }
        } else if (allNull == 1) {
          holder->portPrevRecv = true;
        }
      }

      if (strcmp(kvp[i].key, "seqNumber") == 0) {
        // if port has *not* been received before, set seqNum to 1, and save
        // the sequence to the same index the port was saved to
        if (!holder->portPrevRecv) {
          holder->seqNum = atoi(kvp[i].value);
          holder->prevSeqNum = holder->seqNum - 1;
          pSeq[saveIndex].ackSeqNumber = atoi(kvp[i].value);

          break;
        }
        // if the port *has* been received before...
        if (holder->portPrevRecv) {

          if (atoi(kvp[i].value) == pSeq[saveIndex].ackSeqNumber) {
            pSeq[saveIndex].ackSeqNumber++;
            sprintf(kvp[i].value, "%d", pSeq[saveIndex].ackSeqNumber);
            holder->seqNum = atoi(kvp[i].value);
            break;
          }

          // if the seqNum is less than that of the one saved, change kvp to
          // the saved seqence num
          if (atoi(kvp[i].value) < pSeq[saveIndex].ackSeqNumber) {
            // if the stored values of seqnum are equal to the previously
            // stored value, and is less than the stored index value...
            if (holder->seqNum == holder->prevSeqNum) {
              //  iterate old stored seq num and set kvp to it
              memset(kvp[i].value, 0, sizeof(kvp[i].value));
              sprintf(kvp[i].value, "%d", (pSeq[saveIndex].ackSeqNumber + 1));
              // increase the stored seq num value to the kvp
              holder->seqNum = atoi(kvp[i].value);
              // re-store seqnum in the index
              pSeq[saveIndex].ackSeqNumber = holder->seqNum;
              break;
            }

            // after removal of the vars below the program still functioned
            // the same. Leaving them for future testing incase another edge
            // case error pops up later down the line

            memset(kvp[i].value, 0, sizeof(kvp[i].value));
            sprintf(kvp[i].value, "%d", pSeq[saveIndex].ackSeqNumber);
            holder->seqNum = atoi(kvp[i].value);
            break;
          }

          // if the received kvp value and seqNum is greater than the stored
          // sequence... need this check because when sending the seq num
          // persists (meaning if I have a seq num of 8 at location 15, and I
          // send from 15 to 1 for the first time, seqNum will be 9 instead of
          // 1)
          if (atoi(kvp[i].value) > pSeq[saveIndex].ackSeqNumber) {
            if (holder->seqNum > pSeq[saveIndex].ackSeqNumber) {
              memset(kvp[i].value, 0, sizeof(kvp[i].value));
              // iterate seqence num and apply to kvp index
              sprintf(kvp[i].value, "%d", (pSeq[saveIndex].ackSeqNumber + 1));
              holder->seqNum = atoi(kvp[i].value);
              holder->prevSeqNum = holder->seqNum - 1;
              pSeq[saveIndex].ackSeqNumber = holder->seqNum;
              break;
            } else {
              // if the port has been received prior and the kvp value is
              // greater than the currently stored seqnum, increase the
              // variables below
              memset(kvp[i].value, 0, sizeof(kvp[i].value));
              // iterate seqence num and apply to kvp index
              sprintf(kvp[i].value, "%d", (pSeq[saveIndex].ackSeqNumber + 1));
              holder->seqNum = atoi(kvp[i].value);
              pSeq[saveIndex].ackSeqNumber = holder->seqNum;
              break;
            }
          }
        }
      }
    }
  }
}
/* [Function Notes]
 * Checks incomming non-ACK messages for port/seq numbers before storing the
 * related values for each drone in a struct
 *
 * Upon msg receival, re-verifies that a seq number for a given port is not
 * larger, smaller, or the same as it was for the previously received message
 * (for that specific port). If it is one of those three incorrect options,
 * the function updates the seq number to the correct sequence value
 */
void portSequence(bool portCheck, int distance, int noOfKVP, kValPairs kvp[],
                  port_seq pSeq[], var_holder *holder) {

  int allNull = 0;
  int saveIndex = 0;
  int portVal = 0;
  int prevSeq = 0;
  holder->portPrevRecv = false;

  for (int i = 0; i < noOfKVP; i++) {

    if (strcmp(kvp[i].key, "fromPort") == 0) {
      for (int j = 0; j < 200; j++) {
        // iterate through to check if port was previously seen
        if (pSeq[j].port == atoi(kvp[i].value)) {
          allNull = 1;
          saveIndex = j; // if port was seen before, save the index
          portVal = atoi(kvp[i].value); // save the port
          break;
        }
        if (pSeq[j].port == 0) {
          // if port has not been received previously
          allNull = 0;
        }
      }

      if (allNull == 0) {
        holder->portPrevRecv = false;
        // ensure space is empty before saving new port
        for (int j = 0; j < 200; j++) {
          if (!pSeq[j].port && !pSeq[j].seqNumber) {
            pSeq[j].port = atoi(kvp[i].value);
            portVal = atoi(kvp[i].value);
            saveIndex = j;
            break;
          }
        }
      } else if (allNull == 1) {
        holder->portPrevRecv = true;
      }
    }

    if (portCheck && (distance > 2)) {
      if (strcmp(kvp[i].key, "seqNumber") == 0) {
        // if port has *not* been received before, set seqNum to 1, and save
        // the sequence to the same index the port was saved to
        if (!holder->portPrevRecv) {
          holder->seqNum = 1;
          holder->prevSeqNum = 0;
          pSeq[saveIndex].seqNumber = holder->seqNum;
          // seqnum doesnt reset on send, it resets on receive
          memset(kvp[i].value, 0, sizeof(kvp[i].value));
          sprintf(kvp[i].value, "%d", pSeq[saveIndex].seqNumber);

          holder->portPrevRecv = true;
          break;
        }

        // if the port *has* been received before...
        if (holder->portPrevRecv) {
          int currSeqNum = atoi(kvp[i].value);

          if (currSeqNum == pSeq[saveIndex].seqNumber) {
            continue;
          }

          if (currSeqNum < pSeq[saveIndex].seqNumber) {
            pSeq[saveIndex].seqNumber = currSeqNum;
          }

          // check if current seq num is greater than previous seq num
          if (currSeqNum == pSeq[saveIndex].seqNumber + 1) {

            pSeq[saveIndex].seqIncreased = true;
            pSeq[saveIndex].seqNumber = currSeqNum;
            holder->seqNum = currSeqNum;
            holder->prevSeqNum = currSeqNum - 1;
          }

          // increase the seq number of the port only once
          if (distance > 2 && !pSeq[saveIndex].seqIncreased) {
            pSeq[saveIndex].seqNumber++;
            pSeq[saveIndex].seqIncreased = true;
          }
        }
      }
    }

    // if it is meant for the machine

    if (portCheck && (distance <= 2 && distance > 0)) {
      if (strcmp(kvp[i].key, "seqNumber") == 0) {

        // if port has *not* been received before, set seqNum to 1, and save
        // the sequence to the same index the port was saved to
        if (!holder->portPrevRecv) {
          holder->seqNum = 1;
          holder->prevSeqNum = 0;
          pSeq[saveIndex].seqNumber = holder->seqNum;
          // seqnum doesnt reset on send, it resets on receive
          memset(kvp[i].value, 0, sizeof(kvp[i].value));
          sprintf(kvp[i].value, "%d", pSeq[saveIndex].seqNumber);
          holder->portPrevRecv = true;

          break;
        }

        // if the port *has* been received before...
        if (holder->portPrevRecv) {

          // if the seqNum is less than that of the one saved, change kvp to
          // the saved seqence num
          if (atoi(kvp[i].value) < pSeq[saveIndex].seqNumber) {
            // if the stored values of seqnum are equal to the previously
            // stored value, and is less than the stored index value...
            if (holder->seqNum == holder->prevSeqNum) {
              // if (holder->seqNum < pSeq[saveIndex].seqNumber) {
              //  iterate old stored seq num and set kvp to it
              memset(kvp[i].value, 0, sizeof(kvp[i].value));
              sprintf(kvp[i].value, "%d", (pSeq[saveIndex].seqNumber + 1));
              // increase the stored seq num value to the kvp
              holder->seqNum = atoi(kvp[i].value);
              // re-store seqnum in the index
              pSeq[saveIndex].seqNumber = holder->seqNum;
              break;
              //}
            }

            // after removal of the vars below the program still functioned
            // the same. Leaving them for future testing incase another edge
            // case error pops up later down the line

            memset(kvp[i].value, 0, sizeof(kvp[i].value));
            sprintf(kvp[i].value, "%d", pSeq[saveIndex].seqNumber);
            holder->seqNum = atoi(kvp[i].value);
            break;
          }

          // if the received kvp value and seqNum is greater than the stored
          // sequence... need this check because when sending the seq num
          // persists (meaning if I have a seq num of 8 at location 15, and I
          // send from 15 to 1 for the first time, seqNum will be 9 instead of
          // 1)
          if (atoi(kvp[i].value) > pSeq[saveIndex].seqNumber) {
            if (holder->seqNum > pSeq[saveIndex].seqNumber) {
              memset(kvp[i].value, 0, sizeof(kvp[i].value));
              // iterate seqence num and apply to kvp index
              sprintf(kvp[i].value, "%d", (pSeq[saveIndex].seqNumber + 1));
              holder->seqNum = atoi(kvp[i].value);
              holder->prevSeqNum = holder->seqNum - 1;
              pSeq[saveIndex].seqNumber = holder->seqNum;
              break;
            } else {
              // if the port has been received prior and the kvp value is
              // greater than the currently stored seqnum, increase the
              // variables below
              memset(kvp[i].value, 0, sizeof(kvp[i].value));
              // iterate seqence num and apply to kvp index
              sprintf(kvp[i].value, "%d", (pSeq[saveIndex].seqNumber + 1));
              holder->seqNum = atoi(kvp[i].value);
              pSeq[saveIndex].seqNumber = holder->seqNum;
              break;
            }
          }
        }
      }
    }
  }
}

/* [Function Notes]
 * checks the validity of version and determines range before deciding if the
 * program should print parsed values
 */
void printMessage(char *argv[], int storedFileLine, int prevVal,
                  bool versionCheck, int distance, int senderLoc,
                  int myLocation, int noOfKVP, kValPairs kvp[],
                  string stringHolder) {
  // Determines what line we read from if fed a file
  // honestly dont know if this is even used anymore, but I'm leaving it
  // in case we need it for future labs to deal with files
  int multiplier = 0;

  // if a line with proper port/version values is found...
  if (versionCheck == true) {

    /* ---- Print in Range, Potentially Out of Grid Messages ---- */
    if (distance > 0 && distance <= 2) {

      // print qualifiers to let user know what is going on
      printf("%s", stringHolder.port);
      printf("%s", stringHolder.version);
      printf("%s", stringHolder.location);
      printf("%s", stringHolder.myLocation);
      printf("%s", stringHolder.ttl);
      printf("| Distance between drones is %i...\n", distance);
      printf("|---------------------------------|\n");

      // equation which decides what k:v pair to start printing from if fed a
      // file (k:v pairs are stored line by line from a file before being
      // printed. This equation catches then line where a proper port is
      // identified
      if (storedFileLine > 0) {
        multiplier = noOfKVP * storedFileLine;
      }

      printf("[IN RANGE]\n");
      // if sender location is outside the bounds of the grid
      if (senderLoc < 0 || senderLoc > (atoi(argv[2]) * atoi(argv[3]))) {
        printf("[NOT IN GRID]\n");
      }

      printf("|%-20s  %-20s\n", "[Name]", "[Value]");
      // point to the start of a line by going to the first of that line's k:v
      // pairs (multiplier), before then iterating up
      for (int i = multiplier; i < (multiplier + noOfKVP); i++) {
        // print the proceding k:v pairs untill the end of the line (as d)
        printf("| %-20s %-20s\n", kvp[i].key, kvp[i].value);
      }

      prevVal = storedFileLine;
      versionCheck = false;
    } else {
      printf("|[OUT OF RANGE]\n");
      return;
    }

    /* ---- Printing Out of Range & Out of Grid Messages ---- */
    if ((senderLoc < 0 && distance > 2) ||
        (senderLoc > (atoi(argv[2]) * atoi(argv[3])) && distance > 2)) {
      printf("[NOT IN GRID]\n");
    }
  } else {
    printf("\n[Client Msg - this line was not intented to be seen by this "
           "server]\n[Check that you are sending the proper version number]\n");
  }
}
/* [Function Notes]
 * THIS FUNCTION IS HIGHLY UNMODULARIZED
 * Determines the message type (ack/regular) and prints or forwards the message
 * depending on various factors like ttl, sequence number, etc.
 */
void readOrForward(bool portCheck, bool versionCheck, kValPairs kvp[],
                   int distance, int storedFileLine, int senderLoc,
                   int myLocation, int noOfKVP, int portNumber, char *argv[],
                   var_holder *holder, struct sockaddr_in serverAddress,
                   int socketDesc, srv_addr srv[], string stringHolder,
                   port_seq pSeq[]) {

  char locationSwitch[10]; // changes location to current drone if forwarding
  char pNum[200];          // temporarily stores drone port to cat send-path
  char
      sendPathArr[200]; // stores the changes to be made to send-path before cat
  char catKVP[200];     // concatenates one kvp at a time into a buffer
  char sendBuffer[200]; // used to send ACKs and forward messages

  int prevVal = -1; // Ensures we dont read the same line in multiple times
  int fromPortHolder = 0;
  int toPortHolder = 0;
  int ttlHolder = 0;

  memset(sendPathArr, 0, sizeof(sendPathArr));
  memset(locationSwitch, 0, sizeof(locationSwitch));
  memset(sendBuffer, 0, sizeof(sendBuffer));
  memset(catKVP, 0, sizeof(sendBuffer));

  // Determines if a message is regular or an ACK
  for (int i = 0; i < (noOfKVP); i++) {

    if (strcmp(kvp[i].key, "type") == 0) {
      if (strcmp(kvp[i].value, "ACK") == 0) {
        holder->ack = true;
      } else {
        printf("'Type' received in message, but not a valid ACK receipt\n");
        holder->ack = false;
      }
    }
  }

  // ------------------- The seq Num is still 2 here -----------------

  // if the buffer contains a regular message...
  if (!holder->ack) {
    portSequence(portCheck, distance, noOfKVP, kvp, pSeq, holder);
  } else if (holder->ack) {
    ackPortSequence(portCheck, distance, noOfKVP, kvp, pSeq, holder);
  }

  // port and version valid - message was meant for this machine
  if ((portCheck && versionCheck)) {

    // if ack message is received and the seqnum < previous iterator,
    // and the seqnum is not larger than kvp...
    for (int i = 0; i < noOfKVP; i++) {
      if (strcmp(kvp[i].key, "seqNumber") == 0) {
        if (holder->ack && (holder->seqNum < holder->prevSeqNum) &&
            (holder->seqNum <= atoi(kvp[i].value))) {
          holder->prevSeqNum = holder->seqNum - 1;
          holder->ack = false;
        }
      }
    }
    // if ack is true and the sequence number is larger than previous received
    // seqnum
    if (holder->ack && holder->seqNum > holder->prevSeqNum) {

      // print the ack message body
      printMessage(argv, storedFileLine, prevVal, versionCheck, distance,
                   senderLoc, myLocation, noOfKVP, kvp, stringHolder);

      // need to reset TTL after it is received here, or the sender cannot
      // receive additional ACKs
      holder->timeTL = TTL;
      holder->ttlPrevious = TTL;
      holder->prevSeqNum = holder->seqNum;

    } else if (holder->ack && (holder->seqNum == holder->prevSeqNum)) {
      printf("|[Drone already received ACK %d]\n", holder->seqNum);
      return;
    }

    // if a regular message is received and the sequence number is larger than
    // the previous number
    if (!holder->ack && holder->seqNum > holder->prevSeqNum) {

      // temp storage for port variables which will be used to reassign ack
      // to/fromPort later
      for (int i = 0; i < noOfKVP; i++) {
        if (strcmp(kvp[i].key, "toPort") == 0) {
          toPortHolder = atoi(kvp[i].value);
        }
        if (strcmp(kvp[i].key, "fromPort") == 0) {
          fromPortHolder = atoi(kvp[i].value);
        }
        if (strcmp(kvp[i].key, "TTL") == 0) {
          ttlHolder = atoi(kvp[i].value);
        }
      }

      /* -- Determine if message is in range and of proper version -- */
      printMessage(argv, storedFileLine, prevVal, versionCheck, distance,
                   senderLoc, myLocation, noOfKVP, kvp, stringHolder);
      // reassign values of, and cat received msg variables to buffer
      // - 1 to get rid of myLocation
      for (int i = 0; i < (noOfKVP - 1); i++) {

        if (strcmp(kvp[i].key, "msg") == 0) {
          continue; // skip
        }

        if (strcmp(kvp[i].key, "location") == 0) {
          memset(kvp[i].value, 0, sizeof(kvp[i].value));
          sprintf(kvp[i].value, "%d", myLocation);
        }

        if (strcmp(kvp[i].key, "fromPort") == 0) {
          memset(kvp[i].value, 0, sizeof(kvp[i].value));
          sprintf(kvp[i].value, "%d", toPortHolder);
        }

        if (strcmp(kvp[i].key, "toPort") == 0) {
          memset(kvp[i].value, 0, sizeof(kvp[i].value));
          sprintf(kvp[i].value, "%d", fromPortHolder);
        }

        if (strcmp(kvp[i].key, "TTL") == 0) {
          memset(kvp[i].value, 0, sizeof(kvp[i].value));
          sprintf(kvp[i].value, "%d", (ttlHolder + 2));
        }

        if (strcmp(kvp[i].key, "send-path") == 0) {

          memset(kvp[i].value, 0, sizeof(kvp[i].value));
          sprintf(kvp[i].value, "%d", portNumber);
          sprintf(holder->sendPath, "%s", kvp[i].value);
        }

        memset(catKVP, 0, sizeof(catKVP));
        // concatenate parsed kvp's back into buffer for sending
        sprintf(catKVP, "%s:%s ", kvp[i].key, kvp[i].value);
        catKVP[strlen(catKVP) + 1] = ' ';
        strcat(sendBuffer, catKVP);
      }

      // cat msg type to buffer
      memset(catKVP, 0, sizeof(catKVP));
      sprintf(catKVP, "%s:%s ", "type", "ACK");
      catKVP[strlen(catKVP) + 1] = ' ';
      strcat(sendBuffer, catKVP);

      // reset ttl counting variables
      holder->ttlExpired = false;
      holder->timeTL = TTL;
      holder->ttlPrevious = TTL;
      if (distance <= 2 && distance > 0) {
        // return ack to sender and zero buffer
        cliSend(srv->noOfSrv, srv, socketDesc, sendBuffer, serverAddress,
                holder, portNumber);
        printf("|\n|[Returned ACK to Sender]\n");
        memset(sendBuffer, 0, sizeof(sendBuffer));
        // iterate prevSeqNum after a message is received
        holder->prevSeqNum = holder->seqNum;
      }

      // else if regular message, and the seqnum has been previously received...
    } else if (!holder->ack && (holder->seqNum == holder->prevSeqNum) &&
               holder->portPrevRecv) {
      printf("|[Drone already received this line]\n");
      return;
    }

    return;
  }

  // if port is wrong but version is valid, forward message
  if (!portCheck && versionCheck) {

    // store current drone's location
    sprintf(locationSwitch, "%d", myLocation);

    // iterate through buffer and find location
    for (int i = 0; i < (noOfKVP - 1); i++) {

      if (strcmp(kvp[i].key, "location") == 0) {
        // change location value to current machine
        strcpy(kvp[i].value, locationSwitch);
      }
      // cat current port onto the end of send path
      if (strcmp(kvp[i].key, "send-path") == 0) {
        sprintf(pNum, "%d", portNumber);
        strcat(sendPathArr, ",");
        strcat(sendPathArr, pNum);
        strcat(kvp[i].value, sendPathArr);
        sprintf(holder->sendPath, "%s", kvp[i].value);
      }

      // concatenate parsed kvp's back into buffer to forward to next drone
      sprintf(catKVP, "%s:%s ", kvp[i].key, kvp[i].value);
      catKVP[strlen(catKVP) + 1] = ' ';
      strcat(sendBuffer, catKVP);
    }
    // send re-catted buffer and inform user
    cliSend(srv->noOfSrv, srv, socketDesc, sendBuffer, serverAddress, holder,
            portNumber);
    printf("|[Forwarded successfully to: %d]\n", holder->portHldr);
    memset(sendBuffer, 0, sizeof(sendBuffer));
    // these two lines are neccessary for getting the ack message to appear on
    // senders end
    holder->timeTL = TTL;
    holder->ttlPrevious = TTL;
    return;
  }
}

/* [Function Notes]
 * Concatenates location of machine onto the end of the buffer;
 *
 * Changes " " space in 'msg' to '^'
 * Changes '^' space in 'msg' to " "
 *
 * Parses through buffer and splits up k:v pairs (stored in struct);
 *
 * If the port and version # are correct, print k:v pairs
 * If port is wrong, detail that the line is for a different srv and forward
 * the message
 */
void inputParser(int socketDesc, int portNumber,
                 struct sockaddr_in serverAddress, char bufferReceived[],
                 srv_addr srv[], int noOfSrv, char *argv[], kValPairs kvp[],
                 var_holder *holder, port_seq pSeq[], resendBuffer *rsBuff) {

  int loopNum = 0;         // What loop we are on or what line we are reading
  int storedFileLine = -1; // Used to store what line in a file we are on (not
                           // necessary for lab3 but kept incase we go back to
                           // the file system)
  int distance;            // Distance between drones
  int noOfKVP = 0;         // Number of kvp in buffer
  int senderLoc = 0;       // Location of sending machine
  int myLocation = 0;      // Location of receiving machine

  int fromPortCount = 0;
  int toPortCount = 0;
  int toPortHolder;
  int versionCount = 0;
  int locationCount = 0;
  int msgCount = 0;
  int sendPathCount = 0;
  int myLocationCount = 0;
  int ttlCount = 0;
  int moveCount = 0;
  int seqNumCount = 0;

  bool versionCheck = false; // Ensures that the message is of the right version
  bool portCheck = false;

  char timeStr[5];

  holder->ttlExpired = false;
  holder->ack = false;
  holder->moveRecv = false;

  string stringHolder; // Stores messages related to qualifiers (i.e.
                       // port/version validation, etc.)

  memset(holder->sendPath, 0, sizeof(holder->sendPath));
  memset(&stringHolder, 0, sizeof(stringHolder));

  // adds a space to the end of a string and concatenates location of
  // receiving machine onto the end of the buffer

  bufferReceived[strlen(bufferReceived)] = ' ';
  for (int i = 0; i < noOfSrv; i++) {
    if (portNumber == srv[i].port) {

      strcat(bufferReceived, "myLocation:");
      strcat(bufferReceived, srv[i].location);
    }
  }

  // Replaces white space with '^' if inbetween " "
  replaceSpace(bufferReceived, '^', '"', ' ');
  // Takes in an initial string to be parsed and returns a pointer to the
  // first char token (returns null if there are no tokens left)
  char *nextKVP = strtok(bufferReceived, " ");

  /* ---- Parsing KVP Buffer ---- */
  while (nextKVP != NULL) {

    // Checks for specific character in the string that nextKVP iterates
    // through to signify what separates a kvp
    char *kvIdentifier = strchr(nextKVP, ':');

    // If a ':' character is identified....
    if (kvIdentifier != NULL) {

      // Copies the string of the key and stores it. Starts from the previous
      // " " space and ends on the kvIdentifier (:). Stores the string in the
      // key or value struct variables at the address determined by "noOfKVP"
      memset(kvp[noOfKVP].key, 0, sizeof(kvp[noOfKVP].key));
      strncpy(kvp[noOfKVP].key, nextKVP, kvIdentifier - nextKVP);

      // [For the Funct Below] - If nextKVP is a space, the function subtracts
      // the distance from the whitespace to the current kvIdentifier to get the
      // value
      memset(kvp[noOfKVP].value, 0, sizeof(kvp[noOfKVP].value));
      strncpy(kvp[noOfKVP].value, kvIdentifier + 1,
              strlen(nextKVP) - (kvIdentifier - nextKVP));

      if (strcmp(kvp[noOfKVP].key, "msg") == 0) {
        msgCount++;
        if (msgCount > 1) {
          printf("[ERROR - drone doesn't support copies of variables]\n");
          break;
        }
      }

      if (strcmp(kvp[noOfKVP].key, "fromPort") == 0) {
        fromPortCount++;
        if (fromPortCount > 1) {
          printf("[ERROR - drone doesn't support copies of variables]\n");
          break;
        }
      }

      if (strcmp(kvp[noOfKVP].key, "send-path") == 0) {
        sendPathCount++;
        if (sendPathCount > 1) {
          printf("[ERROR - drone doesn't support copies of variables]\n");
          break;
        }
      }

      if (strcmp(kvp[noOfKVP].key, "seqNumber") == 0) {
        seqNumCount++;
        if (seqNumCount > 1) {
          printf("[ERROR - drone doesn't support copies of variables]\n");
          break;
        }
      }

      if (strcmp(kvp[noOfKVP].key, "move") == 0) {
        moveCount++;
        if (moveCount > 1) {
          printf("[ERROR - drone doesn't support copies of variables]\n");
          break;
        }
        holder->moveRecv = true;
        holder->moveLocation = atoi(kvp[noOfKVP].value);
      }

      /* ---- Port and Version Check ---- */
      // if the port key is found, cross reference it with the drone's
      // initialized port, if they match, store the line of the file that we
      // are currently on
      if (strcmp(kvp[noOfKVP].key, "toPort") == 0) {
        toPortCount++;
        if (toPortCount > 1) {
          printf("[ERROR - drone doesn't support copies of variables]\n");
          break;
        }

        // check if the associated value pair of the key matches server port
        toPortHolder = atoi(kvp[noOfKVP].value);
        int port = atoi(kvp[noOfKVP].value);
        if (port == portNumber) {
          // if server port matches, store the line that we are currently on
          strcpy(stringHolder.port, "| Port validated ...\n");
          storedFileLine = loopNum; // (currently useless for lab 3+)
          portCheck = true;

        } else {
          strcpy(stringHolder.port, "|[Invalid Server Port Provided]\n");
          portCheck = false;
        }
      }

      // if the version key is found, check if the value is of the proper
      // version, if it is return true
      if (strcmp(kvp[noOfKVP].key, "version") == 0) {
        versionCount++;
        if (versionCount > 1) {
          printf("[ERROR - drone doesn't support copies of variables]\n");
          break;
        }
        // check if the associated value pair matches version #
        int version = atoi(kvp[noOfKVP].value);
        if (version == 8) {
          // if version # matches, return true
          strcpy(stringHolder.version, "| Version validated ...\n");
          versionCheck = true;
        } else {
          strcpy(stringHolder.version, "|[Invalid Version Provided]\n");
          versionCheck = false;
        }
      }
      /* -------------------------------- */

      /* ---- Gather Location(s) for Distance Calculations ---- */
      if (strcmp(kvp[noOfKVP].key, "location") == 0) {
        locationCount++;
        if (locationCount > 1) {
          printf("[ERROR - drone doesn't support copies of variables]\n");
          break;
        }
        senderLoc = atoi(kvp[noOfKVP].value);
        sprintf(stringHolder.location, "| Location1 found at %i...\n",
                senderLoc);
      }

      if (strcmp(kvp[noOfKVP].key, "myLocation") == 0) {
        myLocationCount++;
        if (myLocationCount > 1) {
          printf("[ERROR - drone doesn't support copies of variables]\n");
          break;
        }
        myLocation = atoi(kvp[noOfKVP].value);
        sprintf(stringHolder.myLocation, "| Location2 found at %i...\n",
                myLocation);
      }

      /* ------------------------------------------------------ */

      /* ---- Messsage TTL Check ---- */
      if (strcmp(kvp[noOfKVP].key, "TTL") == 0) {
        ttlCount++;
        if (ttlCount > 1) {
          printf("[ERROR - drone doesn't support copies of variables]\n");
          break;
        }
        holder->timeTL = atoi(kvp[noOfKVP].value);
        holder->ttlPrevious = holder->timeTL;
      }

      /* ---------------------------- */
    }

    // Replaces '^' spaces with ' ' if inbetween " "
    replaceSpace(kvp[noOfKVP].value, ' ', '"', '^');

    nextKVP = strtok(NULL, " ");
    noOfKVP++;
  }
  /* ------------------------------ */

  /* ---- Calculating Distance ---- */
  // All indexes start at the 0th position
  // Subtracting 1 from the location places it in the proper "grid" space

  int x1 = 0;
  int y1 = 0;
  int x2 = 0;
  int y2 = 0;

  // Modulo (%) calculates column position (if we were in a 5x5 grid, this
  // makes sure we reset to first column after we reach the 6th location )
  y1 = ((senderLoc - 1) % atoi(argv[2])) + 1;
  // Division determines row - in a 5x5 grid, until the
  // 6th location this would be 0
  x1 = ((senderLoc - 1) / atoi(argv[2])) + 1;
  y2 = ((myLocation - 1) % atoi(argv[2])) + 1;
  x2 = ((myLocation - 1) / atoi(argv[2])) + 1;

  // NOTE - the calculation below doesnt work flawlessly. With the way modulo
  // operates, sometimes the program produces false positives, like location
  // 24 being able to send to location 40 in a 6x7 grid

  // [euclidian distance formula calculations]
  distance = trunc(sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2) * 1.0));
  holder->distance = distance;

  char sendBuffer[200];
  char catKVP[50];
  memset(sendBuffer, 0, sizeof(sendBuffer));

  for (int i = 0; i < (noOfKVP - 1); i++) {

    if (strcmp(kvp[i].key, "TTL") == 0) {
      if (distance <= 2 && distance > 0) {
        holder->timeTL--; // decrement time to live tracker
        // places new value in key's value variable
        sprintf(timeStr, "%d", holder->timeTL);
        strcpy(kvp[i].value, timeStr);

        if (holder->timeTL > 0) {
          sprintf(stringHolder.ttl, "| Message TTL is %i...\n", holder->timeTL);
        } else {
          sprintf(stringHolder.ttl, "|[Message Expired]");
        }
      }
    }
    memset(catKVP, 0, sizeof(catKVP));
    // concatenate parsed kvp's back into buffer for sending
    sprintf(catKVP, "%s:%s ", kvp[i].key, kvp[i].value);
    catKVP[strlen(catKVP) + 1] = ' ';
    strcat(sendBuffer, catKVP);
  }

  char *type = strstr(sendBuffer, "type:");
  for (int i = 0; i < 50; i++) {
    // if buffer index is empty and msg is not ack or move
    if (rsBuff->buffer[i] == 0 && holder->moveRecv == 0 && type == NULL) {

      sprintf(&rsBuff->buffer[i], "%s", sendBuffer);

      holder->ttlPrevious++;
      break;
    } else if (rsBuff->buffer[i] != 0) {
      int toPortInt = 0;
      char *toPort = strstr(&rsBuff->buffer[i], "toPort:");

      if (toPort != NULL) {
        toPort += strlen("toPort:");
        char *endPort = strchr(toPort, ' ');
        if (endPort == NULL) {
          endPort = toPort + strlen(toPort);
        }
        // gather the int value of toPort key
        toPortInt = strtol(toPort, &endPort, 10);
      }

      for (int j = 0; j < MAXSRV; j++) {
        // if the toPort is found in the server list, set sequence
        // number to that server's current iteration and inccrease
        // by 1
        if (srv[j].port == toPortInt) {
          holder->seqNum = srv[j].seqNumber;
          holder->seqNum += 1;
          srv[j].seqNumber = holder->seqNum;
          char *seqNum = strstr(&rsBuff->buffer[i], "seqNumber:");

          if (seqNum != NULL) {
            seqNum += strlen("seqNumber:");
            char *endSeqNum = strchr(seqNum, ' ');

            if (endSeqNum == NULL) {
              endSeqNum = seqNum + strlen(seqNum);
            }
            // replace old sequence number value beforesend
            char buff[12];
            snprintf(buff, sizeof(buff), "%d", holder->seqNum);
            memcpy(seqNum, buff, strlen(buff));
            memcpy(seqNum + strlen(buff), endSeqNum, strlen(endSeqNum) + 1);
          }
        }
      }

      if (toPortInt == toPortHolder) {
        memset(&rsBuff->buffer[i], 0, sizeof(rsBuff->buffer[i]));
        sprintf(&rsBuff->buffer[i], "%s", sendBuffer);

        holder->ttlPrevious++;
        holder->timeTL++;
        break;
      } else {

        break;
      }
    } else {
      continue;
    }
  }

  if (portCheck && versionCheck) {
    for (int i = 0; i < MAXSRV; i++) {
      if (srv[i].port == portNumber && (holder->moveRecv)) {
        printf("\n|----- Network Select Popped -----|\n");

        printf("|[Move command received]\n");
        sprintf(srv[i].location, "%i", holder->moveLocation);
        printf("|[Changed drone's location to %s]\n", srv[i].location);

        for (int i = 0; i < 50; i++) {
          // if the index i of buffer array is not empty...
          if (rsBuff->buffer[i] != 0) {
            int toPortInt = 0;
            char *toPort = strstr(&rsBuff->buffer[i], "toPort:");

            char *ttl = strstr(&rsBuff->buffer[i], "TTL:");
            if (ttl != NULL) {
              ttl += strlen("TTL:");
              char *endTTL = strchr(ttl, ' ');

              if (endTTL == NULL) {
                endTTL = ttl + strlen(ttl);
              }
              // gather the int value of TTL key
              int ttlValue = strtol(ttl, &endTTL, 10);
              ttlValue++;

              if (ttlValue > 0) {
                char buff[12];
                snprintf(buff, sizeof(buff), "%d", ttlValue);
                memcpy(ttl, buff, strlen(buff));
                memcpy(ttl + strlen(buff), endTTL, strlen(endTTL) + 1);
              } else {
                memset(&rsBuff->buffer[i], 0, sizeof(rsBuff->buffer[i]));
                break;
              }

              if (&rsBuff->buffer[i] != NULL && ttlValue <= 2) {
                memset(&rsBuff->buffer[i], 0, sizeof(rsBuff->buffer[i]));
                printf("|[Message Buffer %d Deleted]\n", i);
                printf("|----------------------------------|\n\n");
                printf("Enter String to Send:\n"); // prompts for user input
                break;
              } else {
                // ensure there is a toPort element
                if (toPort != NULL) {
                  toPort += strlen("toPort:");
                  char *endPort = strchr(toPort, ' ');
                  if (endPort == NULL) {
                    endPort = toPort + strlen(toPort);
                  }
                  // gather the int value of toPort key
                  toPortInt = strtol(toPort, &endPort, 10);
                } else {
                  break;
                }

                for (int j = 0; j < MAXSRV; j++) {
                  // if the toPort is found in the server list, set sequence
                  // number to that server's current iteration and inccrease
                  // by 1
                  if (srv[j].port == toPortInt) {
                    holder->seqNum = srv[j].seqNumber;

                    srv[j].seqNumber = holder->seqNum;
                    char *seqNum = strstr(&rsBuff->buffer[i], "seqNumber:");

                    if (seqNum != NULL) {
                      seqNum += strlen("seqNumber:");
                      char *endSeqNum = strchr(seqNum, ' ');

                      if (endSeqNum == NULL) {
                        endSeqNum = seqNum + strlen(seqNum);
                      }
                      // replace old sequence number value beforesend
                      char buff[12];

                      snprintf(buff, sizeof(buff), "%d", holder->seqNum);
                      memcpy(seqNum, buff, strlen(buff));
                      memcpy(seqNum + strlen(buff), endSeqNum,
                             strlen(endSeqNum) + 1);
                    }
                  }
                }

                // send cmd line input to all servers in config file

                printf("|[Re-sent Buffer]\n");
                cliSend(srv->noOfSrv, srv, socketDesc, &rsBuff->buffer[i],
                        serverAddress, holder, portNumber);
                break;
              }
            }

          } else {
            continue;
          }
        }

        printf("|---------------------------------|\n\n");
        printf("Enter String to Send:\n"); // prompts for user input
        return;
      }
    }
  }

  if (!portCheck && holder->moveRecv) {
    printf("\n|----- Network Select Popped -----|\n");
    printf("|[Move command destined for another drone]\n");
    printf("|---------------------------------|\n\n");
    printf("Enter String to Send:\n"); // prompts for user input

    return;
  }

  if (!portCheck && distance > 2) {
    printf("\n|----- Network Select Popped -----|\n");
    printf("|[Distance too large to receive message]\n");
    printf("|---------------------------------|\n\n");
    printf("Enter String to Send:\n"); // prompts for user input
    return;
  }

  if (distance == 0) {
    return;
  }

  /*------------------------------*/

  memset(bufferReceived, 0, 200);

  // if ttl has decremented and distance > 0
  if (holder->timeTL < holder->ttlPrevious && distance > 0) {
    // if ttl not expired and ttl > 0
    if (!holder->ttlExpired && (holder->timeTL > 0)) {
      printf("\n|----- Network Select Popped -----|\n");

      // if within range and ttl is still alive...
      readOrForward(portCheck, versionCheck, kvp, distance, storedFileLine,
                    senderLoc, myLocation, noOfKVP, portNumber, argv, holder,
                    serverAddress, socketDesc, srv, stringHolder, pSeq);
      printf("|---------------------------------|\n\n");
      printf("Enter String to Send:\n"); // prompts for user input
      holder->ttlPrevious = holder->timeTL;
      return;
    } else {

      printf("|[Message TTL Expired]\n");
      holder->ttlExpired = false;
      holder->timeTL = TTL;
      holder->ttlPrevious = TTL;
      return;
    }
  }

  loopNum++;
}

/***********************************************************************/

/***********************************************************************/
/* -------------------------- Main Functions ------------------------- */
/***********************************************************************/

/* [Function Notes]
 * Concatenates all kvps in a message besides toPort, msg, and optional move
 * command
 */
void concatenateVars(char *userInput, srv_addr *srv, int portNumber,
                     char *timeToLive, var_holder holder, char tempArr[]) {

  for (int i = 0; i < srv->noOfSrv; i++) {
    if (portNumber == srv[i].port) {
      userInput[strlen(userInput) - 1] = ' ';

      strcat(userInput, "fromPort:");
      sprintf(tempArr, "%d", srv[i].port);
      strcat(userInput, tempArr);

      userInput[strlen(userInput)] = ' ';
      strcat(userInput, "version:");
      strcat(userInput, "8");

      userInput[strlen(userInput)] = ' ';
      strcat(userInput, "location:");
      strcat(userInput, srv[i].location);

      userInput[strlen(userInput)] = ' ';
      strcat(userInput, "TTL:");
      strcat(userInput, timeToLive);

      userInput[strlen(userInput)] = ' ';
      strcat(userInput, "send-path:");
      strcat(userInput, tempArr);
    }
  }
}

int main(int argc, char *argv[]) {

  srv_addr srv[MAXSRV]; // struct to carry ip, port, location from config
  fd_set socketFDS;     // a collection of socket file descriptors the server is
                        // maintaining
  kValPairs kvp[200];
  struct sockaddr_in serverAddress; // server gnode address

  char userInput[200]; // input buffer used for concatenation of location
  char timeToLive[10];
  char *loc = "";

  int socketDesc = 0; // socket descriptor
  int portNumber = 0; // get this as an argument from the command line
  int maxSD;
  int selectRC;

  bool portHasSent = false;

  bool msgSent = false;

  parseConfig("config.file", srv, MAXSRV);

  var_holder holder;
  resendBuffer rsBuff[100];
  port_seq pSeq[200];

  holder.timeTL = TTL;
  holder.ttlPrevious = TTL;
  holder.prevSeqNum = 0;
  holder.seqNum = 1;
  holder.ack = false;

  struct timeval timeout;

  memset(pSeq, 0, sizeof(pSeq));
  memset(rsBuff, 0, sizeof(rsBuff));

  sprintf(timeToLive, "%d", holder.timeTL);

  startUpDrone(&socketDesc, &portNumber, argc, argv, serverAddress,
               srv->noOfSrv, srv, loc, socketFDS);

  printf("Enter String to Send:\n"); // prompts for user input

  /* ---- Infinite Loop for Gathering/Sending Msg ---- */
  for (;;) {

    timeout.tv_sec = 20;
    timeout.tv_usec = 0;

    // while time to live is valid...
    while (holder.timeTL > 0) {

      char tempArr[1000];
      memset(userInput, 0, 200); // clears buffer
      memset(tempArr, 0, sizeof(tempArr));

      // setup input and check for output from cmd line
      FD_SET(fileno(stdin), &socketFDS);
      // check for output from the network
      FD_SET(socketDesc, &socketFDS);

      // determine the maximum file descriptors that an fd_set object can hold
      if (fileno(stdin) > socketDesc) {
        maxSD = fileno(stdin); // set to cmd line input
      } else {
        maxSD = socketDesc; // set to network response
      }

      /* ---- Select Functionality ---- */
      // Allows the server to monitor multiple client connections and listen
      // for input while checking for errors
      selectRC = select(maxSD + 1, &socketFDS, NULL, NULL, &timeout);
      if (selectRC == -1) {
        perror("[ERROR - select issue]");
      }

      // ---------- KEYBOARD INPUT RECEIVED ----------
      // i.e. drone wants to send something...
      if (FD_ISSET(fileno(stdin), &socketFDS)) {
        printf("\n|---- Keyboard Select Popped ----|\n");

        // assign input from cmd line to buffer and check for errors
        if (fgets(userInput, sizeof(userInput), stdin) == NULL) {
          perror("|[fgets ptr ERROR]");
          printf("|--------------------------------|\n\n");
          exit(1);
        }

        char *toPort = strstr(userInput, "toPort:");
        char *msg = strstr(userInput, "msg:");

        int toPortInt = 0;

        // check that the entered stdin has proper variables before moving
        // forward
        if (msg == NULL) {
          printf("|[ERROR - no msg specified]\n\n");
          printf("|--------------------------------|\n\n");
          break;
        }
        if (toPort == NULL) {
          printf("|[ERROR - no toPort specified]\n\n");
          printf("|--------------------------------|\n\n");
          break;
        } else {
          toPort += strlen("toPort:");
          char *endPort = strchr(toPort, ' ');
          if (endPort == NULL) {
            endPort = toPort + strlen(toPort);
          }
          // determine value of toPort kvp
          toPortInt = strtol(toPort, &endPort, 10);
        }

        // set the sequence number for each drone
        for (int i = 0; i < MAXSRV; i++) {
          if (srv[i].port == toPortInt) {
            holder.seqNum = srv[i].seqNumber;
          }
        }

        // if the drone has previously sent a message, run this the next time
        // keyboard input is received
        if (selectRC == 1 && msgSent) {

          // concatenate the location, version, and port of the sending
          // machine cat time to live for the message
          concatenateVars(userInput, srv, portNumber, timeToLive, holder,
                          tempArr);
          holder.seqNum++;

          // increase the sent sequence number
          userInput[strlen(userInput)] = ' ';
          strcat(userInput, "seqNumber:");
          memset(tempArr, 0, sizeof(tempArr));
          sprintf(tempArr, "%d", holder.seqNum);
          strcat(userInput, tempArr);

          // save new sequence number
          for (int i = 0; i < MAXSRV; i++) {
            if (srv[i].port == toPortInt) {
              srv[i].seqNumber = holder.seqNum;
            }
          }

          holder.timeTL = TTL;
          holder.ttlPrevious = TTL;
          holder.ack = false;

          // send cmd line input to all servers in config file
          cliSend(srv->noOfSrv, srv, socketDesc, userInput, serverAddress,
                  &holder, portNumber);
          holder.prevSeqNum = holder.seqNum;
          printf("|Successfully sent updated buffer\n");
          printf("|--------------------------------|\n\n");
          char *move = strstr(userInput, "move:");
          if (move != NULL) {
            msgSent = false;
          } else {
            msgSent = true;
          }

          for (int i = 0; i < 50; i++) {
            int portCompare = 0;
            char *toPort = strstr(&rsBuff->buffer[i], "toPort:");
            if (toPort != NULL) {
              toPort += strlen("toPort:");
              char *endPort = strchr(toPort, ' ');
              if (endPort == NULL) {
                endPort = toPort + strlen(toPort);
              }
              // gather the int value of toPort key
              portCompare = strtol(toPort, &endPort, 10);
            }

            if (portCompare == toPortInt) {
              memset(&rsBuff->buffer[i], 0, sizeof(rsBuff->buffer[i]));
              sprintf(&rsBuff->buffer[i], "%s", userInput);
              break;
            } else if (rsBuff->buffer[i] == 0 && move == NULL) {
              sprintf(&rsBuff->buffer[i], "%s", userInput);
              break;
            }
          }

          msgSent = true;

          timeout.tv_sec = 20;
          timeout.tv_usec = 0;
          printf("Enter String to Send:\n"); // prompts for user input
          break;
        }
        // if never sent anything
        else if ((selectRC == 1 && holder.seqNum <= 1)) {

          // printf("\nCP:3.2\n");
          holder.ack = false;
          holder.seqNum = 1;
          holder.prevSeqNum = 0;

          // concatenate the location, version, port, ttl, send path, and
          // seqNum of the sending machine
          concatenateVars(userInput, srv, portNumber, timeToLive, holder,
                          tempArr);

          userInput[strlen(userInput)] = ' ';
          strcat(userInput, "seqNumber:");
          memset(tempArr, 0, sizeof(tempArr));
          sprintf(tempArr, "%d", holder.seqNum);
          strcat(userInput, tempArr);

          holder.timeTL = TTL;
          holder.ttlPrevious = TTL;

          char *move = strstr(userInput, "move:");

          for (int i = 0; i < 50; i++) {
            if (rsBuff->buffer[i] == 0 && move == NULL) {
              sprintf(&rsBuff->buffer[i], "%s", userInput);
              break;
            } else {
              continue;
            }
          }
          // send cmd line input to all servers in config file
          cliSend(srv->noOfSrv, srv, socketDesc, userInput, serverAddress,
                  &holder, portNumber);

          portHasSent = true;
          holder.prevSeqNum = holder.seqNum;

          for (int i = 0; i < MAXSRV; i++) {
            if (srv[i].port == toPortInt) {
              srv[i].seqNumber = holder.seqNum;
            }
          }

          printf("|Sent buffer for the first time\n");
          printf("|--------------------------------|\n\n");
          if (move != NULL) {
            msgSent = false;
          } else {
            msgSent = true;
          }

          timeout.tv_sec = 20;
          timeout.tv_usec = 0;

          printf("Enter String to Send:\n"); // prompts for user input

          break;
        }
      }

      // ---------- NETWORK INPUT RECEIVED ----------
      if (FD_ISSET(socketDesc, &socketFDS)) { // if network input is received

        char *tempBuff;       // gather buffer
        socklen_t fromLength; // defines the length of buffer
        fromLength = sizeof(serverAddress);

        // check if we can receive the input from the network
        tempBuff = srvReceive(socketDesc, userInput, serverAddress, fromLength);

        // parse through and print the input if valid (among other things)
        inputParser(socketDesc, portNumber, serverAddress, tempBuff, srv,
                    srv->noOfSrv, argv, kvp, &holder, pSeq, rsBuff);

        // if time to live runs out, reset vars before returning to the
        // beginning of the for loop

        memset(userInput, 0, 200);
        timeout.tv_sec = 20;
        timeout.tv_usec = 0;
        break;
      }

      // ---------- TIMEOUT OCCURRED ----------
      if (selectRC == 0) {

        bool allEmpty = true;
        for (int i = 0; i < 50; i++) {
          if (rsBuff->buffer[i] != 0) {
            allEmpty = false;
            break;
          }
        }

        if (allEmpty) {
          break;
        }

        // if a message hasnt been sent yet, wait
        if (!msgSent) {
          break;
        } else {

          for (int i = 0; i < 50; i++) {
            // if the index i of buffer array is not empty...
            if (rsBuff->buffer[i] != 0) {

              int toPortInt = 0;
              char *toPort = strstr(&rsBuff->buffer[i], "toPort:");

              char *ttl = strstr(&rsBuff->buffer[i], "TTL:");
              if (ttl != NULL) {
                ttl += strlen("TTL:");
                char *endTTL = strchr(ttl, ' ');

                if (endTTL == NULL) {
                  endTTL = ttl + strlen(ttl);
                }
                // gather the int value of TTL key
                int ttlValue = strtol(ttl, &endTTL, 10);
                // printf("\nTTL VALUE: %d\n", ttlValue);
                if (ttlValue > 0) {
                  ttlValue--;
                  char buff[12];
                  snprintf(buff, sizeof(buff), "%d", ttlValue);
                  memcpy(ttl, buff, strlen(buff));
                  memcpy(ttl + strlen(buff), endTTL, strlen(endTTL) + 1);
                }

                if (&rsBuff->buffer[i] != NULL && ttlValue <= 2) {
                  memset(&rsBuff->buffer[i], 0, sizeof(rsBuff->buffer[i]));
                  printf("\n|-------- Timeout Occurred --------|\n");
                  printf("|[Message Buffer %d Deleted]\n", i);
                  printf("|----------------------------------|\n\n");
                  timeout.tv_sec = 20;
                  timeout.tv_usec = 0;
                  printf("Enter String to Send:\n"); // prompts for user input

                  break;
                } else {
                  // ensure there is a toPort element
                  if (toPort != NULL) {
                    toPort += strlen("toPort:");
                    char *endPort = strchr(toPort, ' ');
                    if (endPort == NULL) {
                      endPort = toPort + strlen(toPort);
                    }
                    // gather the int value of toPort key
                    toPortInt = strtol(toPort, &endPort, 10);
                  } else {
                    break;
                  }

                  for (int j = 0; j < MAXSRV; j++) {
                    // if the toPort is found in the server list, set sequence
                    // number to that server's current iteration and inccrease
                    // by 1
                    if (srv[j].port == toPortInt) {
                      printf("HOLDER SEQNUM: %d\n", holder.seqNum);
                      printf("SRV SEQNUM: %d\n", srv[j].seqNumber);
                      holder.seqNum = srv[j].seqNumber;

                      holder.seqNum += 1;
                      srv[j].seqNumber = holder.seqNum;
                      char *seqNum = strstr(&rsBuff->buffer[i], "seqNumber:");

                      if (seqNum != NULL) {
                        seqNum += strlen("seqNumber:");
                        char *endSeqNum = strchr(seqNum, ' ');

                        if (endSeqNum == NULL) {
                          endSeqNum = seqNum + strlen(seqNum);
                        }
                        // replace old sequence number value beforesend
                        char buff[12];
                        snprintf(buff, sizeof(buff), "%d", holder.seqNum);
                        memcpy(seqNum, buff, strlen(buff));
                        memcpy(seqNum + strlen(buff), endSeqNum,
                               strlen(endSeqNum) + 1);
                      }
                    }
                  }

                  // send cmd line input to all servers in config file
                  printf("\n|-------- Timeout Occurred --------|\n");
                  printf("|[Re-sent Buffer]\n");
                  printf("%s\n", &rsBuff->buffer[i]);
                  printf("|----------------------------------|\n\n");
                  printf("Enter String to Send:\n"); // prompts for user input
                  cliSend(srv->noOfSrv, srv, socketDesc, &rsBuff->buffer[i],
                          serverAddress, &holder, portNumber);
                  // break;
                  msgSent = true;
                  break;
                }
              }

            } else {
              continue;
            }
          }

          timeout.tv_sec = 20;
          timeout.tv_usec = 0;
          break;
        }
      }

      if (holder.ttlExpired) {
        holder.timeTL = TTL;
        holder.ttlPrevious = TTL;
        continue;
      }
    }
  }
}