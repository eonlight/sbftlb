# !/bin/bash

if [ $# -lt 2 ]; then
	echo "Usage: $0 [Arq] [Tests]"
else

echo $1 >> results.txt
echo $1 >> sd.txt

if [ $2 -gt 0 ]; then
echo 100 bytes
python tester.py 0 0 0 4
fi 
if [ $2 -gt 1 ]; then
echo 1 Kbyte
python tester.py 0 0 1 4
fi
if [ $2 -gt 2 ]; then
echo 10 Kbytes
python tester.py 0 0 2 4
fi
if [ $2 -gt 3 ]; then
echo 100 Kbytes
python tester.py 0 0 3 4
fi
fi
