#include <IRremote.h>
#include <IRremoteInt.h>

int RECV_PIN = 4;
int codeType = -1;
IRrecv irrecv(RECV_PIN);
IRsend irsend; // send pin hardcoded to pin 3.
decode_results results;

long serial_recd_code;

#define DEBUG_MODE 0
#define PRINT_MODE HEX
//#define PRINT_MODE BYTE

int loop_count = 0;

void send_code_to_serial(long code) {
  Serial.print(code, PRINT_MODE);
  Serial.print(' ');
  Serial.print(~code, PRINT_MODE);
#ifdef DEBUG_MODE
  Serial.print(' ');
  Serial.println(code + ~code, HEX);
#endif
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

void send_code_to_ir(long code) {
#ifdef DEBUG_MODE
    Serial.print("Sending: ");
    Serial.println(code, HEX);
#endif
    irsend.sendNEC(reverse_code(code), 32);
    //delay(100);
    //irsend.sendNEC(code, 32);
    
    irrecv.enableIRIn(); // send disables recv.
}

long get_code_from_serial() {
 
  long code = 0;
  long notcode = 0;
  if ( Serial.available() >= 12 ) {
    // we possibly have a packet.
    // starts with 4 0x00's.
    for (int i; i<4; i++) {
      if ( Serial.read() != 0x00 ) {
        return 0;
      }
    }

    // cool, got 4-bytes of 0x00, next 4 bytes is code.
    code &= Serial.read() << 24;
    code &= Serial.read() << 16;
    code &= Serial.read() << 8;
    code &= Serial.read();

    // next 4 bytes should be logical-not of code
    notcode &= Serial.read() << 24;
    notcode &= Serial.read() << 16;
    notcode &= Serial.read() << 8;
    notcode &= Serial.read();

    // confirm that we've got valid data
    if ( code + notcode == 0xffffffff ) {
        return code;
    }

  }

  return 0;
}

void setup()
{
  Serial.begin(9600);
  irrecv.enableIRIn(); // Start the receiver 
  Serial.println("Start");
}

void loop() {
  loop_count++;
  
  int zero_count = 0;
  
  //serial_recd_code = get_code_from_serial();
  //if ( serial_recd_code ) {
  //   send_code_to_ir(serial_recd_code);
  //}

  // if we've got data from IR, spit to serial
  //if (irrecv.decode(&results) && results.decode_type == NEC) {
  /*
    if (irrecv.decode(&results) ) {
    Serial.println("Gots data");
    if (results.value == REPEAT) {
      // Don't record a NEC repeat value as that's useless.
      Serial.println("repeat; ignoring.");
    } else {
      Serial.println(reverse_code(results.value), HEX);
      // IRremote lib returns the code LSB first, so reverse it.
      send_code_to_serial(reverse_code(results.value));
    }
    irrecv.resume(); // Receive the next value
  }
*/
  if ( loop_count == 200 ) {
    // give badge an ID.
    send_code_to_ir(0xbb5309aa);
    //send_code_to_serial(0xbb5309aa);
    Serial.println("Woop;");
  } else if ( loop_count == 1000 ) {
    // get badge to send data
    //send_code_to_serial(0xbb530800);
  }

  if ( loop_count % 1000 == 0 ) {    
    loop_count = 0;
  }
  
  //send_code_to_ir(0xbb530980);
  delay(2);

}

