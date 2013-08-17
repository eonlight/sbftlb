#!/bin/bash

# 1 thread aguenta 30000 pacotes +/-
# 10 = 300000

#tests
if [ $1 -eq 50 ]; then
echo "Coping lbs to s3"
sshpass -p fabiim scp lb/*.c s3.quinta:/home/ruben/SBFTLB/
sshpass -p fabiim scp lb/*.h s3.quinta:/home/ruben/SBFTLB/
echo "Coping server to s5"
sshpass -p fabiim scp server/*.c  s5.quinta:~/ruben
sshpass -p fabiim scp server/*.h  s5.quinta:~/ruben
fi

if [ $1 -eq 20 ]; then
echo "Coping $2 to s5"
sshpass -p fabiim scp $2 s5.quinta:~/ruben
fi

if [ $1 -eq 30 ]; then
echo "Coping $2 to s3"
sshpass -p fabiim scp $2 s3.quinta:/home/ruben/SBFTLB/
fi

if [ $# -eq 2 ] && [ $1 -eq 0 ]; then
echo "Coping $2 to s3"
sshpass -p fabiim scp $2 s3.quinta:/home/ruben/SBFTLB/
echo "Coping $2 to s9"
sshpass -p fabiim scp $2 s9.quinta:/home/ruben/SBFTLB/
echo "Coping $2 to s5"
sshpass -p fabiim scp $2 s5.quinta:/home/ruben/SBFTLB/
fi

if [ $# -eq 2 ] && [ $1 -eq 1 ]; then
echo "Coping $2 to r3"
sshpass -p quinta scp $2 r3.quinta:
echo "Coping $2 to r4"
sshpass -p fabiim scp $2 r4.quinta:
echo "Coping $2 to r5"
sshpass -p fabiim scp $2 r5.quinta:
fi

if [ $1 -eq 3 ] || [ $1 -eq -1 ]; then
echo "Coping lbs to s3"
sshpass -p fabiim scp lb/*.c s3.quinta:/home/ruben/SBFTLB/
sshpass -p fabiim scp lb/*.h s3.quinta:/home/ruben/SBFTLB/
echo "Coping lbs to s9"
sshpass -p fabiim scp lb/*.c s9.quinta:/home/ruben/SBFTLB/
sshpass -p fabiim scp lb/*.h s9.quinta:/home/ruben/SBFTLB/
echo "Coping lbs to s5"
sshpass -p fabiim scp lb/*.c s5.quinta:/home/ruben/SBFTLB/
sshpass -p fabiim scp lb/*.h s5.quinta:/home/ruben/SBFTLB/
fi

if [ $1 -eq 3 ] || [ $1 -eq -2 ]; then
echo "Coping server to r3"
sshpass -p quinta scp server/*.c r3.quinta:
sshpass -p quinta scp server/*.h r3.quinta:	
echo "Coping server to r4"
sshpass -p fabiim scp server/*.c r4.quinta:
sshpass -p fabiim scp server/*.h r4.quinta:
echo "Coping server to r5"
sshpass -p fabiim scp server/*.c r5.quinta:
sshpass -p fabiim scp server/*.h r5.quinta:
fi
