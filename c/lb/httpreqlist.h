#ifndef HTTPREQLIST_H
#define HTTPREQLIST_H

typedef struct request_node{
  char *buffer;
  int server; //server id
  time_t added;
  int len; //length of the buffer
  int blooms; //num blooms received from 'server'
  struct request_node *next;
  struct request_node *prev;
} HttpRequestNode;

void cleanList(HttpRequestNode *head);
HttpRequestNode * removeFromList(HttpRequestNode *current);
void addToList(int lb, int len, unsigned char * buffer, int server);
void addNodeToList(int lb, HttpRequestNode *node);

#endif