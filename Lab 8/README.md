#### Title: CSCI 3762 - Network Programming - Lab 8

#### Name: Samuel Hilfer

#### Due: 04/17/2023

## Message Storage - Lab 8

### To-Do:				

| Requrements                                                               | Completed |
|---------------------------------------------------------------------------|-----------|
| Lab 7 (and prior) functionality                                           | √         |
| Drones store received buffers                                             | √         |
| Drones send stored buffers on move                                        | √         |
| After receiving/sending a message, drones resend buffers every 20 sec     | √         |
| On resend and move drones decrement TTL                                   | √         |
| Drones do not save ACK messages (optional)                                | √         |
| When message TTL = 0, program deletes saved buffer                        | √         |

## Code updates from the previous lab...
	
There are quite a few notable updates from the code of the previous lab. First and foremost, drones now individually store message buffers they have either sent themselves, or received from other drones. Additionally they resend these buffers every 20 seconds, or if the drone is moved to a different location.

Additionally, ACK messages are not included when saving buffers, and if a buffer TTL hits 0, the buffer gets erased to free up space for other potential messages.

---------------------------------------------------------------------------

### [NOTES FOR RUNTIME]
	
I have not changed anything regarding runtime between this and the previous lab. Review notes below for a refresher 
         
> ---------------------------------------------------------------------------         
>           {Everything below this line is runtime notes from lab 6 + 7}
> ---------------------------------------------------------------------------
       
I have not changed anything regarding runtime between this and the previous lab. The executed 'make' command is still 'gcc -o drone7 drone7.c -lm', and I have not changed any of the variables the user needs to enter on the command line. The 'msg' and 'toPort' kvps are requred to be input before each send, and the 'move' command is optional.       
       
With version 6 of the drone, I have run into an issue with my makefile. Though the commands I am using are technically correct, and though the same makefile worked with lab 5 (drone5), there is now an error that pops up when running "make" that states that one of my variables is set but never used. This may sound like a simple issue, but its not. That's because this variable is absolutely being used in multiple spots throughout the function it is defined within. I was unable to figure out why exactly the make command is producing this error, though I was able to narrow the issue down to the CFLAGs (either -g or -Wall) being the issue. This is quite confusing to me, because previously in the semester I was using those two flags to make my program without any errors.
    
My solution to this was to simply remove the CFLAGS, which means my makefile now runs without them, but it also makes properly. For easy reference, below are the two commands that are/were being used by the makefile.
    
        Old:
            gcc -g -Wall drone6.c -o drone6 -lm
    
    
        New:
            gcc -o drone6 drone6.c -lm
         

---------------------------------------------------------------------------
### Output:
![Samuel Hilfer - Output](https://github.com/GrowingPaigns/Network-Drones/assets/63205868/0afeb458-ecdc-4032-b053-054bb9cbba60)

### Issues during testing on (05/01/2023):

I had a slight issue where I was incrementing the seqNumber of messages when a move command was received, rather than just sending the buffer. Fixed this by 10:00 AM and was able to get the proper output immediately after
    
### Issues Remaining in the program (04/14/2023)
    

There are a lot of bugs remaining in the program, in lab 7, the main issue users could trigger was the sending of multiple of the same kvp. I fixed this in lab 8 by sending an error for duplicate KVPs, but there are a whole slew of other issues that came with the save buffer and move/timeout functionality we implemnted. For example, one of the new main issues is that, though I could get the proper output, once that message TTL ticks down to 0 from the timeouts which deletes the buffer, the program has an issue sending subsequent messages, and essentially soft-locks. 

Also, though you dont see it in my output, when I send buffers, I currently incremen the sequence number of each buffer, while simultaneously decrementing TTL. This was not a requirement, and in fact, is against what was supposed to be implemented which is that the drone simply resends the buffers with the same seqNumber until TTL = 0, forcing the receiving drones to deal with the duplicate messages being sent


> ---------------------------------------------------------------------------         
>                       {Issue Notes from Lab 7}
> ---------------------------------------------------------------------------

As far as I am aware, there are no glaring* issues in my code. The sending, receiving, forwarding, and moving functionality seems to operate without fault. I am quite excited by this, as it seems with the optimization we did in this lab, I can no longer cause the multiple edge case errors I was experiencing in the previous lab. Buffer overflow cases seem to also have been handled, as after inputting multiple lines of random text, the programs all remain functional (though they don't return any sort of message to inform the user that they're outputting garbage).
      
*[All that said, I have been able to segment fault my program if multiple of the same kvp are sent (i.e. msg:"1" msg:"2" toPort...). Even in this this case though, only the drone that first receives it will crash (i.e if the sender is close enough for the recipient to directly accept it, or if the recipient tries to forward the message). This is still obviously quite a large issue that needs to be addressed, but ultimately it shouldn't cause a problem for any normal buffers being sent.]
