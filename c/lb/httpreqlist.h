#ifndef HTTPREQLIST_H
#define HTTPREQLIST_H

typedef struct request_node{
  char *buffer; //the packet
  //int server; //server id
  time_t added; //add timestamp
  int len; //length of the buffer
  int blooms; //num blooms received from the server
  //int bag; //in which bag this packet was received
  struct request_node *next;
  struct request_node *prev;
} HttpRequestNode;

void cleanList(HttpRequestNode *head);
HttpRequestNode * removeFromList(HttpRequestNode *current);
HttpRequestNode * createNode(int len, char *buffer);
void addToList(HttpRequestNode *current, HttpRequestNode *node);

#endif
