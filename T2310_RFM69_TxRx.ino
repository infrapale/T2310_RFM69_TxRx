/**
T2310RFM69_TxRx 
Send and receive data via UART

https://github.com/infrapale/T2310_RFM69_TxRx

Set Mode
  <Mx> 
    x = J -> json
    x = R -> Raw message  (default)
Send Raw message:
  abc123
Send json split message
  "OD_1;Temp;23.1;-"  
  ->
  "{"Z":"OD_1","S":"Temp","V":23.1,"R":"-"}"
Receive Raw Message
  abc123
Receive json message
  "{"Z":"OD_1","S":"Temp","V":23.1,"R":"-"}"
  ->
  "OD_1;Temp;23.1;-"  




**/

// Define message groups to be supported (Astrid.h)
#include <AstridAddrSpace.h>

#include <Arduino.h>
#include <Wire.h>
#include <RH_RF69.h>
#include <SPI.h>
#include <VillaAstridCommon.h>
#include <SimpleTimer.h> 
#include <secrets.h>



#define ZONE  "OD_1"
#define MINUTES_BTW_MSG   10


//*********************************************************************************************
// *********** IMPORTANT SETTINGS - YOU MUST CHANGE/ONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NETWORKID         100  //the same on all nodes that talk to each other
#define NODEID            10  
#define BROADCAST         255
#define MAX_MESSAGE_LEN   68
#define RECEIVER          BROADCAST    // The recipient of packets
//Match frequency to the hardware version of the radio on your Feather
#define FREQUENCY         RF69_433MHZ
//#define FREQUENCY       RF69_868MHZ
//#define FREQUENCY       RF69_915MHZ
//#define ENCRYPTKEY        "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HCW       true // set to 'true' if you are using an RFM69HCW module

//*********************************************************************************************
#define SERIAL_BAUD   9600

// Change to 434.0 or other frequency, must match RX's freq!
#define RF69_FREQ     434.0  //915.0
#define RFM69_CS      10
#define RFM69_INT     2
#define RFM69_IRQN    0  // Pin 2 is IRQ 0!
#define RFM69_RST     9
//#define ENCRYPTKEY    "VillaAstrid_2003" //exactly the same 16 characters/bytes on all nodes!
#define ENCRYPTKEY    RFM69_KEY   // defined in secret.h

#define LED           13  // onboard blinky

//unit_type_entry Me ={"MH1T1","Terminal","T171","T171","T171","17v01",'0'}; //len = 9,5,5,5,9,byte
time_type MyTime = {2017, 1,30,12,05,55}; 
int16_t packetnum = 0;  // packet counter, we increment per xmission
boolean msgReady;       //Serial.begin(SERIAL_BAUD
boolean SerialFlag;
RH_RF69 rf69(RFM69_CS, RFM69_INT);

SimpleTimer timer;
char radio_packet[MAX_MESSAGE_LEN];
//char radio_packet[RH_RF69_MAX_MESSAGE_LEN];
//sensors_event_t Sensor1; 
byte rad_turn = 0;
byte read_turn = 0;
uint8_t eKey[] ="VillaAstrid_2003"; //exactly the same 16 characters/bytes on all nodes!
byte msg_interval_minutes = MINUTES_BTW_MSG ;

void setup() {
    //while (!Serial); // wait until serial console is open, remove if not tethered to computer
    delay(2000);
    Serial.begin(9600);
    //Smart.begin(9600);
    Serial.println("T187 RFM69HCW Radio Sensor");
  
    // Hard Reset the RFM module
    pinMode(LED, OUTPUT);
    pinMode(RFM69_RST, OUTPUT);
    digitalWrite(RFM69_RST, LOW);
    
    Serial.println("RFM69 TX Test!");
    digitalWrite(RFM69_RST, HIGH);    delay(100);
    digitalWrite(RFM69_RST, LOW);    delay(100);

    if (!rf69.init()) {
       Serial.println("RFM69 radio init failed");
       while (1);
    }
    Serial.println("RFM69 radio init OK!");
    // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
    // No encryption
    if (!rf69.setFrequency(RF69_FREQ)) {
       Serial.println("setFrequency failed");
    }
    // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
    // ishighpowermodule flag set like this:
    rf69.setTxPower(20, true);  // range from 14-20 for power, 2nd arg must be true for 69HCW
 
    uint8_t key[] ="VillaAstrid_2003"; //exactly the same 16 characters/bytes on all nodes!   
    rf69.setEncryptionKey(key);
  
    
    Serial.print("RFM69 radio @");  Serial.print((int)RF69_FREQ);  Serial.println(" MHz");

    //timer.setInterval(10, run_10ms);
    timer.setInterval(1000, run_1000ms);
    timer.setInterval(10000, run_10s);
}

void send_rfm69_message(String str)
{
  char cbuff[MAX_MESSAGE_LEN];
    str.toCharArray(cbuff,MAX_MESSAGE_LEN);
    Serial.println(cbuff);
    radiate_msg(cbuff);
}

void receive_rfm69_message(void)
{
    if (rf69.available()) 
    {
        uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);
        if (rf69.recv(buf, &len)) 
        {
            if (len > 0)
            {
                buf[len] = 0;
                Serial.print("Received [");
                Serial.print(len);
                Serial.print("]: ");
                Serial.println((char*)buf);
                Serial.print("RSSI: ");
                Serial.println(rf69.lastRssi(), DEC);
            }
        }
    }
}


void loop() {
    timer.run();
    byte i;
    String buff;
    char cbuff[80];
    String jstr;
    String vstr;

    buff = Serial.readStringUntil('\n');
    if (buff.length()> 0)
    {
        buff.remove(buff.length()-1);
        send_rfm69_message(buff);
    }
    receive_rfm69_message();

    convert_sensor_json_data("OD_1;Temp;23.1;-",&jstr);
    Serial.println(jstr);

    parse_sensor_json_str(jstr,&vstr);
    Serial.println(vstr);
}

int ConvertFloatSensorToJsonRadioPacket(char *zone, char *sensor, float value, char *remark ){
    byte i;
    unsigned int json_len;
    //Serial.println("ConvertFloatSensorToJson");
    String JsonString; 
    JsonString = "{\"Z\":\"";
    JsonString += zone;
    JsonString += "\",";
    JsonString += "\"S\":\"";
    JsonString += sensor;  
    JsonString += "\",";
    JsonString += "\"V\":";
    JsonString += value;
    JsonString += ",";
    JsonString += "\"R\":\"";
    JsonString += remark;
    JsonString += "\"}";
    
    //Serial.println(JsonString);
    json_len = JsonString.length();
    if (json_len <= MAX_MESSAGE_LEN){
       for (i=0;i<MAX_MESSAGE_LEN;i++)radio_packet[i]=0;
       JsonString.toCharArray(radio_packet,MAX_MESSAGE_LEN);
       Serial.println(json_len);
       return( json_len );
    }
    else {
      Serial.print("JSON string was too long for the radio packet: "); 
      Serial.println(json_len);
      return(0);
    }
}

void convert_sensor_json_data(String sensor_values, String *json_str)
{
    //  Input:  "OD_1;Temp;23.1;-"
    //  Output: "{"Z":"OD_1","S":"Temp","V":23.1,"R":"-"}"
    uint8_t indx1;
    uint8_t indx2;
    indx1 = 0;
    indx2 = sensor_values.indexOf(';');
    *json_str = "{\"Z\":\"";
    *json_str += sensor_values.substring(indx1,indx2);
    *json_str += "\",\"S\":\"";
    indx1 = indx2+1;
    indx2 = sensor_values.indexOf(';',indx1+1);
    *json_str += sensor_values.substring(indx1,indx2);
    *json_str += "\",\"V\":";
    indx1 = indx2+1;
    indx2 = sensor_values.indexOf(';',indx1+1);
    *json_str += sensor_values.substring(indx1,indx2);
    *json_str += ",\"R\":\"";
    indx1 = indx2+1;
    indx2 = sensor_values.indexOf(';',indx1+1);
    *json_str += sensor_values.substring(indx1,indx2);
    *json_str += "\"}";
}

void parse_sensor_json_str(String json_str, String *value_str)
{
    //  json_str: "{"Z":"OD_1","S":"Temp","V":23.1,"R":"-"}"
    // ->
    //  value_str:  "OD_1;Temp;23.1;-"
    *value_str = parse_json_tag(json_str, "{\"Z");
    *value_str += ';';
    *value_str += parse_json_tag(json_str, ",\"S");
    *value_str += ';';
    *value_str += parse_json_tag(json_str, ",\"V");
    *value_str += ';';
    *value_str += parse_json_tag(json_str, ",\"R");

}


String parse_json_tag(String json_str, char *tag){
   int pos1;
   int pos2;
   //Serial.println(fromStr); Serial.println(tag);
   pos1 = json_str.indexOf(tag);
   if (pos1 >= 0){
       if (tag == ",\"V"){
          pos1 = json_str.indexOf(":",pos1) +1;
          if (json_str.charAt(pos1) == '\"') pos1++;
          pos2 = json_str.indexOf(",",pos1);
          if (json_str.charAt(pos2-1) == '\"') pos2--;
       }
       else{
          pos1 = json_str.indexOf(":\"",pos1) +2;
          pos2 = json_str.indexOf("\"",pos1);
       }
       return(json_str.substring(pos1,pos2)); 
   }
   else {
      return("");
   }
    
}

void radiate_msg( char *radio_msg )
{
     
    if (radio_msg[0] != 0){
       Serial.println(radio_msg);
       rf69.send((uint8_t *)radio_msg, strlen(radio_msg));
       rf69.waitPacketSent();
      
    }
}

void Blink(byte PIN, byte DELAY_MS, byte loops)
{
    for (byte i=0; i<loops; i++) {
        digitalWrite(PIN,HIGH);
        delay(DELAY_MS);
        digitalWrite(PIN,LOW);
        delay(DELAY_MS);
    }
}

void run_10s(void)
{
   //ReadSensors();
}


void run_1000ms(void)
{
   //Serial.println(MyTime.second);
   if (++MyTime.second > 59 ){
      MyTime.second = 0;
      if (++msg_interval_minutes >= MINUTES_BTW_MSG) {
            msg_interval_minutes = 0;
            transmit_one_msg();
      }
      if (++MyTime.minute > 59 ){    
         MyTime.minute = 0;
         if (++MyTime.hour > 23){
            MyTime.hour = 0;
         }
      }   
   }
}

void transmit_one_msg(void)
{
   if (++rad_turn > 6) rad_turn = 1;
   float Temp1 = 22.7;
   switch(rad_turn){
   case 1:
      if (ConvertFloatSensorToJsonRadioPacket(ZONE,"Temp",Temp1,"") > 0 ) radiate_msg(radio_packet);
      break;
    }
}
