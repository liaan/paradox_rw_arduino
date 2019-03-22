#define CLK 3 // Keybus Yellow
#define DTA 4 // Keybus Green
#include <SPI.h>
//#include <avr/pgmspace.h>
//#include "C:\path\to\local\folder\CRC32.h"
#define DEVICEID "0952"

char hex[] = "0123456789abcdef";
String st, old;
unsigned long lastData;
char buf[100];
/*********** VARS ********************/
const int MqttZoneUpdateInterval = 1000;
const int MqttAlarmUpdateInterval = 1000;

int SentIncrement = 0;

bool NewKeyStroke = false;

bool SendingKeyStroke = false;
bool BusIdle = false;
bool SendingBits = false;
unsigned int Key = 0;


String BusMessage = "";
String ResponseMessage = "";
String Response = "";
word OutputString;
byte ZoneStatus[33];
byte OldZoneStatus[33];

unsigned long LastClkSignal = 0;

unsigned long lastZoneUpdate = 0;
unsigned long lastAlarmUpdate = 0;
//unsigned long lastZoneUpdate = 0;

typedef  struct  {
  int status;

} struct_alarm_status;

struct_alarm_status AlarmStatus;
struct_alarm_status OldAlarmStatus;

void setup()
{
  pinMode(CLK, INPUT);
  pinMode(DTA, INPUT);
//  pinMode(DTA, OUTPUT);
  //digitalWrite(CLK, HIGH);
  //digitalWrite(DTA, HIGH);

  Serial.begin(115200);
  while (!Serial) {
    // ; // wait for serial port to connect. Needed for native USB
  }


  Serial.println("Booting");

  // attachInterrupt(digitalPinToInterrupt(3),interuptClockFalling,FALLING);
  attachInterrupt(digitalPinToInterrupt(3), interuptClockChange, CHANGE);

  Serial.println("Ready!");
}
void loop()
{
  static bool oldBusIdle = false;

  // ## Check if there anything new on the Bus
  if (checkClockIdle()) {
    BusIdle = true;
   // Serial.println("Idle");
  }else {
    BusIdle = false;
   // Serial.println("Busy");
  }
  
  
  
  

  //If return message is less than 2, then just return as its not complete yet
  if(BusIdle and BusMessage.length() > 2){
     // ## Save message and clear buffer
      String Message = BusMessage ;
      Response = ResponseMessage ;
      // ## Clear old message
      BusMessage = "";
      ResponseMessage  = "";

      //Process Message
      decodeMessage(Message);
  }

  if(BusIdle != oldBusIdle){
    oldBusIdle = BusIdle;
    
    if(BusIdle){
      
      Process_keystroke();       
    }
  }
  


  if (Serial.available() > 0 and Key==0) {
    
      int SerialKey = (Serial.read() - 48);
      Key=SerialKey  ;
      //S => Stay
      if(SerialKey==(86-48)){
        Serial.print("V pressed");
        Key = 163; //Stay
      }
      
      //If 0, then send 10
      if(SerialKey==(48-48)){
        Key = 10;
      }

      //Newline/enter
      if(SerialKey == (10-48)){
        Key = 0;        
      }
      
      Serial.print("Key Pressed:");
      Serial.println(Key,DEC);
    
  }

  
 

}

void Process_keystroke(){
      static int retry_count = 0;
        //Nothing to send
      if(Key == 0){
        retry_count = 0;
        return;
      }
      
      retry_count++;
      
      if(retry_count >=5){
        Serial.print("Retry count exceeded:");
        Serial.println(retry_count);
        
        retry_count = 0;
        Key = 0;
        Serial.print("Resetting key, key now:");
        Serial.println(Key,DEC);
        return;
        Serial.println("Never suppose to see me:");
        
      }
    
      
      //Serial.print("Sending Key:");
      //Serial.println(Key,DEC);
      
      //Should not be needed as only suppose to be called if buss idle;
      while(SendingBits);
      
      //Serial.println("Making Ooutput");
      OutputString = Key;
      //Shift it along
      OutputString = OutputString << 8;
      //Zone
      OutputString |= B00000001;   
      //delay(5);  

      return;
    
}

void  check_keystroke_sent(){
  
}
void decodeMessage(String &msg) {
  //Get command
  int cmd = GetIntFromString(msg.substring(0, 8));
  
  //Some commands have trailing "00 00 00 00" so remove
  if(cmd == 0xD0 || cmd == 0xD1){
      //Strip last 00's
      msg = msg.substring(0, msg.length()-(4*8)-1);
      
      if(!check_crc(msg)){
         Serial.println("CRC Faied:");
        printSerial(msg, HEX);
        return;
      }
      
  }else{
    
      if(!check_crc(msg)) {
         Serial.println("CRC Faied:");
        printSerial(msg, HEX);
        return;
      }
  }
  
  //Serial.println(cmd);

 // printSerial(msg, HEX);
 // printSerial(msg, BIN);

  switch (cmd) {
    case 0xD0: //Zone Status Message
      processZoneStatus(msg);
      break;
    case 0xD1://Seems like Alarm status
      processAlarmStatus(msg);

      break;
    case 0xD2: //Action Message;
      Serial.println("Action");
      printSerial(msg, HEX);
      break;

    case 0x20: //
      Serial.println("Think button press");
      printSerial(msg, HEX);
      break;
    case 0xE0: // Status.
      //Serial.println("Status");
      //printSerial(msg, HEX);
      //Serial.println("0xe0");
      break;
    case 0x0:  //dunno
      break;
      
    case 0x31:  //KeyAck
     // AwaitingAck = false;
      SendingKeyStroke = false;
      Key = 0;
      Serial.println("KeyPress ACKED");
      Serial.println((String) Response);
      printSerial(Response, HEX);
      printSerial(msg, HEX);
      break;

    default:
      //Serial.println("Dunno");
      //printSerialFull(msg);
     // printSerial(msg, HEX);
     // printSerial(msg, BIN);
     // printSerial(msg, 1000);
     // Serial.println((String) Response);

      break;
      ;
  }
  //Serial.print("Cmd=");
  //Serial.println(cmd,HEX);

}


/*******************************************************************************************************/
/****************************** Process Zone Status connect *****************************************/
/**
 * All ok
 * 11010000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000001 01100011
 * Zone 1
 * 11010000 00000000 01000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000001 10111001   
 * Zone 1 and 2 and 3
 * 11010000 00000000 01010100 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000001 1010 0001
 */
void processZoneStatus(String &msg) {
  //Zone 1 = bit 17
  //Zone 2 = bit 19
  //Zone 3 = bit 21

  // Serial.println("ProcessingZones");
   //printSerialFull(msg);

  for (int i = 0; i < 32; i++) {
    //Serial.println(17+(i*2));

    if (msg[17 + (i * 2)] == '1') {
      ZoneStatus[i] = 1;
      Serial.println("Zone:"+(String)(i+1)+"Active");

    } else {

      ZoneStatus[i] = 0;
    }
    // Serial.print("ZoneStatus:"+(String)(i+1)+":");Serial.println(ZoneStatus[i+1],BIN);
  }

}

/****************************** Process Alarm (D1) Status connect *****************************************/
/*
 * Might be first 5 bytes are Partition1 and 2nd 5 bytes is partion 2??
 * Alarm Not set: 11010001 00000000 00000000 00010001 00000000 00000000 00000000 01000100 00000000 00000000 00000001 01001111
 * Alarm Set:     11010001 00000000 01000000 00010001 00000000 00000000 00000000 00000100 00000000 00000000 00000001 01110101  
 * Alarm STay     11010001 00000000 00000000 00010001 00000000 00000000 00000100 00000100 00000000 00000000 00000001 10110000
 * Alarm Sleep    11010001 00000000 00000100 00010001 00000000 00000000 00000000 00000100 00000000 00000000 00000001 00001101
 *                 
 *
 */

void processAlarmStatus(String &msg) {

//  AlarmStatus.status = (msg[(8 + 8 +8 +8 + 8 8 + 8 + 1)] == '1') ? 1 : 0;

  //If set , then get status
  if(msg[((8*7) + 1)] == '0'){

      //Sleep
      if(msg[((8*2) + 5)] == '1'){
        
        AlarmStatus.status = 20;
      }
      //Stay
      if(msg[((8*6) + 5)] == '1'){
        AlarmStatus.status = 30;
      }

      //Full Arm
      if( msg[((8*2) + 1)] == '1' ){
        //Exit Delay
        if(msg[((8*2) + 0)] == '1'){
          AlarmStatus.status = 40;
        }else 
        //Full Alarm
        if(msg[((8*2) + 0)] == '0'){
          AlarmStatus.status = 49;
        } else{
          
          AlarmStatus.status = 45;
        }

        
      }
      
  }else{
    //Not Set
    AlarmStatus.status = 10;
  }
  
  //Only print if not set
  if(AlarmStatus.status != 10){
    Serial.print("Alarm Status: ");Serial.println(AlarmStatus.status);
    printSerial(msg, BIN);
  }
}

/**
 * CRC8
 * 
 * Do Maxim crc8 calc
 */
uint8_t crc8( uint8_t *addr, uint8_t len)
{
     uint8_t crc=0;
     
     for (uint8_t i=0; i<len;i++) 
     {
           uint8_t inbyte = addr[i];
           for (uint8_t j=0;j<8;j++) 
           {
                 uint8_t mix = (crc ^ inbyte) & 0x01;
                 crc >>= 1;
                 if (mix) 
                       crc ^= 0x8C;
                 
                 inbyte >>= 1;
           }
     }
     return crc;
}

/**
 * Check CRC
 * Check if CRC is valid
 * 
 */
uint8_t check_crc(String &st){
  
 // printSerial(st);
 //

  String  val = "";
  int Bytes =   (st.length()) / 8;
  uint8_t calcCRCByte;

  
  //Serial.print("Bytes :");Serial.println(Bytes);
  //printSerialHex(st);
    
  //Make byte array    
  uint8_t* BinnaryStr = strToBinArray(st);    
  
  uint8_t CRC = BinnaryStr[Bytes-1];
  
  calcCRCByte = crc8(BinnaryStr,(int) Bytes-1);
  
  //Serial.print("Crc :");Serial.print((int)CRC,HEX);

  //Serial.print("CrcCalc :");Serial.print((int)calcCRCByte,HEX);
  //Serial.println("");
     
   

  return calcCRCByte == CRC;

  
}

void printSerial(String &st, int Format)
{
  //Get number of Bytes
  int Bytes = (st.length()) / 8;

  String  val = "";

  for (int i = 0; i < Bytes; i++)
  {
    val = st.substring((i * 8), ((i * 8)) + 8);
    if (Format == 1000) {
      Serial.print(GetIntFromString(val));
    } else if (Format == BIN) {
      Serial.print(val);
    } else {
      Serial.print(GetIntFromString(val), Format);
    }
    Serial.print(" ");

  }
  Serial.println("");
}


void printSerialFull(String &st)
{
  //Get number of Bytes
  int Bytes = (st.length()) / 8;

  String  val = "";
  int m = 0;

  for (int i = 0; i < Bytes; i++)
  {
    val = st.substring((i * 8), ((i * 8)) + 8);
    m += GetIntFromString(val);

  }
  Serial.println(m);
}



bool checkClockIdle() {

  unsigned long currentMicros = micros();
  //time diff in
  long idletime = (currentMicros - LastClkSignal);

  if (idletime > 5000) {

    //Serial.println("Idle:"+(String)idletime +":"+currentMicros+":"+ LastClkSignal);
    return true;
  } else {

    return false;
  }
}



void interuptClockChange()
{
  //Falling Edge
  if (digitalRead(CLK) == LOW) {
    //Make sure we listening
    digitalWrite(DTA, HIGH);
    pinMode(DTA, INPUT);
   
   
    DDRD = 0;
    //## Set last Clock time
    LastClkSignal = micros();

    /*
    * Code need to be updated to ignore the response from the keypad (Rising Edge Comms).
    *
    * Panel to other is on Clock Falling edge, Reply is after keeping DATA low (it seems) and then reply on Rising edge
    */

    //Just add small delay to make sure DATA is already set, each clock is 500 ~microseconds, Seem to have about 50ms delay before Data goes high when keypad responce and creating garbage .
    delayMicroseconds(150);
    if (!digitalRead(DTA)) BusMessage += "1"; else BusMessage += "0";
    //Serial.println(digitalRead(DTA));
    if (BusMessage.length() > 200)
    {
      Serial.println("String to long");
      //Serial.println((String) BusMessage);
      BusMessage = "";
      //    printSerialHex(BusMessage);
      return; // Do not overflow the arduino's little ram
    }
  }

  delayMicroseconds(20);
  //Rasing Edge --> device to Panel
  if (digitalRead(CLK) == HIGH) {
    
    if(OutputString > 0 ){
         
        if(!SendingBits){
          //Lock process
          SendingBits = true;
          delayMicroseconds(20);
          //Someone already sending so return;
          if(digitalRead(DTA) == LOW){
            SendingBits = false;
            OutputString = 0;
            SentIncrement = 0;
            digitalWrite(DTA, HIGH);
            pinMode(DTA, OUTPUT);  
            return;
          }
          digitalWrite(DTA, HIGH);
          pinMode(DTA, OUTPUT);  
          
         digitalWrite(DTA, ((OutputString << SentIncrement)&32768) ?LOW:HIGH);          
         SentIncrement++;   
         delayMicroseconds(450);  
         digitalWrite(DTA, HIGH);    
         pinMode(DTA, INPUT);
         
         SendingBits = false;     
        }
        
       if(SentIncrement >= 16){
          SentIncrement = 0;
          //Reset
          OutputString = 0;
          SendingKeyStroke = false;
         // Serial.println("sending done");
          return;
       }
        

        
    }else{
        
        digitalWrite(DTA, HIGH);
        pinMode(DTA, INPUT); 
        
        delayMicroseconds(150);
        if (!digitalRead(DTA)) ResponseMessage += "1"; else ResponseMessage += "0";
    
      if (BusMessage.length() > 200)
      {
        
        Serial.println("String to long");
        //Serial.println((String) ResponseMessage);
        ResponseMessage = "";
        //    printSerialHex(BusMessage);
        return; // Do not overflow the arduino's little ram
      }
      
    
  }
  }

}

/**
 *  waitCLKChange
 *
 *  Wait for 5000ms
 *
 */
unsigned long waitCLKchange(int currentState)
{
  unsigned long c = 0;
  while (digitalRead(CLK) == currentState)
  {
    delayMicroseconds(10);
    c += 10;
    if (c > 10000) break;
  }
  return c;
}
String formatDisplay(String &st)
{
  String res;
  res = st.substring(0, 8) + " " + st.substring(8, 9) + " ";
  int grps = (st.length() - 9) / 8;
  for (int i = 0; i < grps; i++)
  {
    res += st.substring(9 + (i * 8), 9 + ((i + 1) * 8)) + " ";
  }
  res += st.substring((grps * 8) + 9, st.length());
  return res;
}
void printSerial(String &st)
{
  //Serial.println(formatSt(st));
  //Serial.println((String)st);
  //Serial.println("Length:"+(String)st.length());
  Serial.print("Normal: ");
  //Get number of Bytes

  int Bytes = (st.length()) / 8;
  String  val = "";

  for (int i = 0; i < Bytes; i++)
  {
    String kk = "012345670123456701234567";
    val = st.substring((i * 8), ((i * 8)) + 8);

    Serial.print(GetIntFromString(val));
    Serial.print(" ");

  }
  Serial.println("");
}
void printSerialHex(String &st)
{
  //Serial.println(formatSt(st));
  //Serial.println((String)st);
  //Serial.println("Length:"+(String)st.length());
  Serial.print("HEX: ");
  //Get number of Bytes

  int Bytes = (st.length()) / 8;
  String  val = "";

  for (int i = 0; i < Bytes; i++)
  {
    String kk = "012345670123456701234567";
    val = st.substring((i * 8), ((i * 8)) + 8);

    Serial.print(GetIntFromString(val), HEX);
    Serial.print(" ");

  }
  Serial.println("");
}
void printSerialDec(String &st)
{

  Serial.print("DEC: ");
  //Get number of Bytes
  int Bytes = (st.length()) / 8;

  String  val = "";

  for (int i = 0; i < Bytes; i++)
  {
    String kk = "012345670123456701234567";
    val = st.substring((i * 8), ((i * 8)) + 8);

    Serial.print(GetIntFromString(val), DEC);
    Serial.print(" ");

  }
  Serial.println("");
}


unsigned int GetIntFromString(String str) {
  int r  = 0;
  // Serial.print("Received:");  Serial.println(str);
  int length = str.length();

  for (int j = 0; j < length; j++)
  {
    if (str[length - j - 1] == '1') {
      r |= 1 << j;
    }

  }

  // Serial.print("Made:");
  // Serial.println(r,BIN);

  return r;
}


unsigned int getBinaryData(String &st, int offset, int length)
{
  int buf = 0;
  for (int j = 0; j < length; j++)
  {
    buf <<= 1;
    if (st[offset + j] == '1') buf |= 1;
  }
  return buf;
}
/**
 * Convert str to binary array
 * 
 */
uint8_t* strToBinArray(String &st)
{
  int Bytes = (st.length()) / 8;
  uint8_t Data[Bytes];
  
  String  val = "";

  for (int i = 0; i < Bytes; i++)
  {
    //String kk = "012345670123456701234567";
    val = st.substring((i * 8), ((i * 8)) + 8);

    Data[i] = GetIntFromString(val);
    

  }

  
  return Data;
}
