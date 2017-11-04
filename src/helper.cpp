#include "helper.h"

// #define DEBUG

FIFO::FIFO()
{
  head = tail = NULL;
}

bool FIFO::isNotEmpty()
{
  if (head==NULL)
    return false;
  else
    return true;
}

void FIFO::addText(String text)
{
  Node * addNode = new Node;
  addNode->label=text;
  addNode->next=NULL;
  if (isNotEmpty())
  {
    tail->next=addNode;
    tail = addNode;
  }
  else
  {
    head=tail=addNode;
  }

  // if (text =="END")
  //   done = true;
  //   else
  //   done =false;

  #ifdef DEBUG
    Serial.print("New List item: ");
    Serial.println(text);
    Serial.print("\r\n");

  #endif

}

String FIFO::getText()
{
  String text = head->label;
  Node * aux = head;
  head = head-> next;
  aux -> next = NULL;
  delete aux;

  #ifdef DEBUG
    Serial.print("Get List item: ");
    Serial.println(text);
    Serial.print("\r\n");

  #endif

  return text;
}
