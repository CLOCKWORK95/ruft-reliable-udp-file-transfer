# RUFT-Reliable-UDP-File-Transfer
Reliable UDP File Transfer Project (Client - Server Web Application)


This is a Computer Engineering University project, implemented using C language on a Unix based operative system.
The intent of this project is to implement a reliable file transfer protocol on the Application Layer of internet protocol stack.
The chosen algorithm to implement reliability makes use of:

a) Sliding windows, on both client and server side, as TCP does at transport layer, to verify the correct ordering of incoming packets.
b) Acknowledgements (Selective Repeat), to ensure protection against packets loss.

Code is organized in Client and Server sides, as a Monolithical architecture.
The Architecture design and the implementation choices are completely original, Server is multi-threaded.
More explainations can be found into the documentation released ( pdf report file (italian) and pptx presentation (english) ).
Thanks a lot, and good coding !!! :D
