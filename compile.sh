# a bash file for compiling mutiple programs
# the option -lrt (use real time library) is needed for mqueue
gcc -Wall -o worker worker.c -lrt
gcc -Wall -o ipc ipc.c -lrt
