#include <Arduino.h>

using namespace std;

struct  Node
{
  String label;
  Node * next;
};

class FIFO
{
private:
  Node * head;
  Node  * tail;

public:
  FIFO();
  void addText(String text);
  String getText();
  bool isNotEmpty();
  // boolean done=false;

};
