#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/**********************************************************************/
/* This is a sample client file. You can use it to make sure you know */
/* how to compile code, learn more about sockets, etc.                */
/* make this code your own. Comment it, understand it, restructure it */
/* I expect to see your create functions and NOT submit this as one   */
/* large main() function                                              */
/**********************************************************************/

/* checks and varifies all reqired variables */
void checkCliVars(int argCount, char *argVec[], int pNum, int scDesc){
  
  int i; /* loop variable */
  struct sockaddr_in checkAddress;
  
  // check to see if the right number of parameters was entered
  if (argCount < 3) {
    printf("\n[ERROR - Client requires more variables: <ipaddr> <portnumber> ]\n");
    exit(1); /* just leave if wrong number entered */
    
  }

  /* what should you do if the socket descriptor is not valid */
  if (scDesc == -1) {
    perror("[ERROR - Socket descriptor invalid]\n");
    exit(1);
  }
  if (scDesc == 0) {
    perror("[ERROR - Socket failed to connect]\n");
    exit(1);
  }
  
  


  //------- added specific checks for different issues ------
  /* this code checks to see if the ip address is a valid ip address */
  /* meaning it is in dotted notation and has valid numbers          */
  
  if (inet_pton(AF_INET, argVec[1], &checkAddress) == -1) {
    printf("[ip ERROR - bad IP address (invalid string entered)]\n");
    exit(1); /* just leave if is incorrect */
  }
  if (inet_pton(AF_INET, argVec[1], &checkAddress) == 0 || inet_pton(AF_INET, argVec[1], &checkAddress) < -1) {
    printf("[ip ERROR - unknown IP Address issue, refer to error code)]\n");
    exit(1); /* just leave if is incorrect */
  }


  
  /* exit if a portnumber too big or too small  */
  if ((pNum > 65535) || (pNum < 0)) {
    printf("[port ERROR - You entered an invalid port number (range: 0-65535)]\n");
    exit(1);
  }
  /* check that the port number is a number..... */
  for (i = 0; i < strlen(argVec[2]); i++) {
    // checks each of the individual chars of argv's Port to see if there are
    // alphabet values
    if (!isdigit(argVec[2][i])) {
      printf("[port ERROR - The port number isn't a valid int value]\n");
      exit(1);
    }
  }
}

/* checks and verifies that there are no problem rc's */
void checkSocketRC(int scDesc, char bffOut[], struct sockaddr_in srvAddr){

  int rc = 0;
  // gathers a socket to send a message to (bufferOut). Gathers the length of
  // the string. The flag (0) allows us to pass extra behaviors if desired.
  // Gather the destination server address and the length of said destination
  // address
  rc = sendto(scDesc, bffOut, strlen(bffOut), 0,
                      (struct sockaddr *)&srvAddr, sizeof(srvAddr));

  /* Check the RC and figure out what to do if it is 'bad' */
  if (rc < strlen(bffOut)) {
    
    perror("[sendto ERROR - Socket variable length issue]");
    exit(1);
  }
  if (rc == -1){
    perror("[sendto ERROR - Failed to send]");
    exit(1);
  }
  
  
}

/*  */
    // argc - argument count (int) stroes number of cmdline aruments passed by user
    // argv - argument vector contains the user passed variables
int main(int argc, char *argv[]) {

  // sockaddr_in specifies a transport address and port for AF_INET
  struct sockaddr_in serverAddress; /* structures for gnode address */
  int socketDesc; /* the socket descriptor gathers destination info */
  // provided by the user on the command line
  char serverIP[20];
  // provided by the user on the command line
  int portNumber = 0;
  // buffer is used to output/input vars (Must clear each time)
  char bufferOut[200];

  // SOCK_DGRAM = datagram service for UDP IP/Port transmission
  socketDesc = socket(AF_INET, SOCK_DGRAM, 0);
  
  // strtol - converts a character string to a long int
  // argv[2] - selects the second variable (port) gathered and stored in "argv"
  // NULL = endptr - points to the first non-int character
  portNumber = strtol(argv[2], NULL, 10);


  // Calls the check for provided variables
  checkCliVars(argc, argv, portNumber, socketDesc);
  /* copy the string value of the ip address into serverIP */
  strcpy(serverIP, argv[1]); 
  serverAddress.sin_family = AF_INET;         /* use AF_INET addresses */
  serverAddress.sin_port = htons(portNumber); /* specify port to listen on */
  serverAddress.sin_addr.s_addr = inet_addr(serverIP); /* binds IP address */

  memset(bufferOut, 0, 100); // clears buffer for use

  // Gathers file from user
  char fileName[50];
  FILE *openFilePTR;
  
  
  // Checks validity of file
  printf("Enter File Name:\n");
  int input = scanf("%s", fileName); 
  if (input != 1){ 
    perror("[ERROR - improper file name]");
    exit(1);
  }
  // Opens file for reading
  openFilePTR = fopen(fileName, "r");
  if (!openFilePTR) {
    perror("[ERROR - failed to open file]");
    exit(1);
  }

  
  ssize_t read;
  size_t length = 200;
  char *line;

  /* Reads file (openFilePTR) to the end of the line, 
     and saves it to the string var (line). Loops for next line */
  while ((read = getline(&line, &length, openFilePTR)) != -1){
    sendto(socketDesc, line, strlen(line), 0, (struct sockaddr *)&serverAddress, sizeof(serverAddress)); 
  }
  
  // Checks return codes
  checkSocketRC(socketDesc, bufferOut, serverAddress);
  

}