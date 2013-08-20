#ifndef CONFIG_H
#define CONFIG_H

#define ID 0
//time (in s) a thread sleeps before trying to send the bloom again
#define WAIT 1
//time before sending blooms
#define RENEW 5
//port where client sends requests
#define FRONT_PORT 8080
//port where servers listen to requests
#define SERVER_PORT 8080
//Hello port of ther protocol
#define HELLO_PORT 33000
//Hello broadcast address
#define HELLO_ADDR "192.168.7.100"
//IP address the client knows
#define FRONT_END "192.168.7.233"
//interface to listen
#define DEV "eth1"

//max tolerated faults
#define FAULTS 1

//bloom protocol port
#define BACK_PORT 32000
//number of packets expected in each bloom
#define BLOOM_LEN 2500000
//max error on the bloom
#define BLOOM_ERROR 0.1

#define LB_NUM 1
char *LBS[] = {"192.168.2.23"};

#endif