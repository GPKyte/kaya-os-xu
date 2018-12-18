# Kaya OS  
A C implementation of Kaya OS for a simplified, academic version of MIPS architecture.  

## Specification and Requirements  
[uMPS2 - Principles of Operation](https://www.google.com)  
[Kaya OS](https://www.google.com)  

## Progress
After the one-time set-ups in test, we created the 8 user processes and performed the 3 Sys 5s per each uProc.
We were able to read in pages from the tape devices and the disk0 device returned successful status codes after writing, but we did not confirm whether the data was a match. This partially succeeded and we got no farther.  
The current code fails during the second user process attempts to write to backing store and gets blocked because a method in the avsl, allocTraffic(), accesses a bad address and causes a kernal panic(). Previously, we had all the uProcs enter and complete the first iteration of read from tape and write to disk0, but afterwards the program entered WAIT() and did not leave.

We did not get to run the actual tests, nor test out virtual memory.

## Contributions  
Feel free to use this code per the license, and do not plagiarize. This project will not accept contributions.  

*AUTHORS*: Ploy Sithisakulrat & Gavin Kyte  
*ADVISOR/CONTRIBUTER*: Michael Goldweber  
