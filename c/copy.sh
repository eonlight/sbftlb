#!/bin/bash

if [ $1 -eq 0 ]; then
echo "Coping lbs to s3"
sshpass -p fabiim scp lb/thread.c s3.quinta:/home/ruben/SBFTLB/
echo "Coping lbs to s9"
sshpass -p fabiim scp lb/thread.c s9.quinta:/home/ruben/SBFTLB/
echo "Coping lbs to s7"
sshpass -p quinta scp lb/thread.c s7.quinta:/home/ruben/SBFTLB/
fi

if [ $1 -eq 1 ]; then
echo "Coping lbs to s3"
sshpass -p fabiim scp lb/forwarder.c s3.quinta:/home/ruben/SBFTLB/
echo "Coping lbs to s9"
sshpass -p fabiim scp lb/forwarder.c s9.quinta:/home/ruben/SBFTLB/
echo "Coping lbs to s7"
sshpass -p quinta scp lb/forwarder.c s7.quinta:/home/ruben/SBFTLB/
fi

if [ $1 -eq 5 ] || [ $1 -eq -1 ]; then
echo "Coping lbs to s3"
sshpass -p fabiim scp lb/*.c s3.quinta:/home/ruben/SBFTLB/
sshpass -p fabiim scp lb/*.h s3.quinta:/home/ruben/SBFTLB/
echo "Coping lbs to s9"
sshpass -p fabiim scp lb/*.c s9.quinta:/home/ruben/SBFTLB/
sshpass -p fabiim scp lb/*.h s9.quinta:/home/ruben/SBFTLB/
#echo "Coping lbs to s5"
#sshpass -p fabiim scp lb/*.c s5.quinta:/home/ruben/SBFTLB/
#sshpass -p fabiim scp lb/*.h s5.quinta:/home/ruben/SBFTLB/
echo "Coping lbs to s7"
sshpass -p quinta scp lb/*.c s7.quinta:/home/ruben/SBFTLB/
sshpass -p quinta scp lb/*.h s7.quinta:/home/ruben/SBFTLB/
fi

if [ $1 -eq 2 ]; then
echo "Coping server to r4"
sshpass -p quinta scp server/*.c r4.quinta:
sshpass -p quinta scp server/*.h r4.quinta:
echo "Coping server to r5"
sshpass -p quinta scp server/*.c r5.quinta:
sshpass -p quinta scp server/*.h r5.quinta:
fi

#sshpass -p quinta scp server/*.c s1.quinta:/home/ruben/SBFTLB/
#sshpass -p quinta scp server/*.h s1.quinta:/home/ruben/SBFTLB/

#sshpass -p quinta scp server/*.c s5.quinta:/home/ruben/SBFTLB/
#sshpass -p quinta scp server/*.h s5.quinta:/home/ruben/SBFTLB/
