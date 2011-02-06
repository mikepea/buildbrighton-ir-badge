#include <IRremote.h>
#include <IRremoteInt.h>

#include "WProgram.h"
void setup();
void loop();
long reverse_code(long code);
int RECV_PIN = 4;
int codeType = -1;
IRrecv irrecv(RECV_PIN);
IRsend irsend;
decode_results results;

int loop_count = 0;



void setup()
{
  Serial.begin(9600);
  irrecv.enableIRIn(); // Start the receiver 
}

void loop() {
  loop_count++;

  if (irrecv.decode(&results) && results.decode_type == NEC) {
    if (results.value == REPEAT) {
      // Don't record a NEC repeat value as that's useless.
      Serial.println("repeat; ignoring.");
    } 
    else {
      Serial.println(reverse_code(results.value), HEX);
    }
    irrecv.resume(); // Receive the next value
  }

  if ( loop_count % 1000 == 0 ) {
    long v = 0xbb530853;  // transfer data mode.
    Serial.print("Sending: ");
    Serial.println(v, HEX);
    irsend.sendNEC(reverse_code(v), 32);
    loop_count = 0;
    irrecv.enableIRIn(); // send disables recv.
  }
  
  delay(10);

}

long reverse_code(long code) {
  long ret = 0x00000000;
  for (int i=0; i<32; i++) {
    if ( bitRead(code, i) == 1 ) {
      bitSet(ret, (31-i));
    }
  }
  return ret;
}

int main(void)
{
	init();

	setup();
    
	for (;;)
		loop();
        
	return 0;
}

