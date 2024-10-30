# Heap_Manager
This is my project on creating a heap manager in C.
The Code contains all the code related to this project .
<br>
To compile and run the code do the following :
<br>
gcc -g -c mm.c -o mm.o
<br>
gcc -g -c testapp.c -o testapp.o
<br>
gcc -g -c gluethread/glthread.c -o gluethread/glthread.o
<br>
After compiling these 3 files you have to do the final compilation : gcc -g testapp.o mm.o gluethread/glthread.o -o exe
