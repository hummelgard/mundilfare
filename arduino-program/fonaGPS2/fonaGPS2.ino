/**
 *
 *
 */
#include <SPI.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#include <avr/interrupt.h>
#include <avr/eeprom.h>

#include "Adafruit_FONA_custom.h"
#include "crc16.h"

char data[100];
int dataIndex=0;
int dataCounter=0;

char    IMEI_id[15] = {0};

uint16_t  batteryLevel;

#define GSM_ONLY true

// Seconds to wait before a new sensor reading is logged.
#define LOGGING_FREQ_SECONDS   18       

// Number of times to sleep (for 8 seconds) 
#define MAX_SLEEP_ITERATIONS_GPS   LOGGING_FREQ_SECONDS / 8  

#define MAX_SLEEP_ITERATIONS_POST  MAX_SLEEP_ITERATIONS_GPS * 10 
#define FONA_POWER_KEY 5



                                                    
// standard pins for the 808 shield
#define FONA_RX 8
#define FONA_TX 9
#define FONA_RST 2



// This is to handle the absence of software serial on platforms
// like the Ardu = 0ino Due. Modify this code if you are using different
// hardware serial port, or if you are using a non-avr platform
// that supports software serial.
#ifdef __AVR__
  #include <SoftwareSerial.h>
  SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
  SoftwareSerial *fonaSerial = &fonaSS;
  SoftwareSerial serialLCD(6, 7);
#else
  HardwareSerial *fonaSerial = &Serial1;
#endif

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);



int sleepIterations = 0;
volatile bool watchdogActivated = true;




// Define watchdog timer interrupt.
ISR(WDT_vect)
{
  // Set the watchdog activated flag.
  // Note that you shouldn't do much work inside an interrupt handler.
  watchdogActivated = true;
}

// Put the Arduino to sleep.
void sleep()
{
  // Set sleep to full power down.  Only external interrupts or 
  // the watchdog timer can wake the CPU! 
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  // Turn off the ADC while asleep.
  power_adc_disable();

  // Enable sleep and enter sleep mode.
  sleep_mode();

  // CPU is now asleep and program execution completely halts!
  // Once awake, execution will resume at this point.
  
  // When awake, disable sleep mode and turn on all devices.
  sleep_disable();
  power_all_enable();
}


void messageLCD(const int time, const String& line1, const String& line2=""){
  serialLCD.write(254); 
  serialLCD.write(128); 
  serialLCD.print("                ");
  serialLCD.write(254); 
  serialLCD.write(192); 
  serialLCD.print("                ");
    delay(10);
  serialLCD.write(124); //max brightness
  serialLCD.write(157);
  delay(10);
  serialLCD.write(254); 
  serialLCD.write(128);
  serialLCD.print(line1);
  serialLCD.write(254); 
  serialLCD.write(192);
  serialLCD.print(line2);  
  if (time > 0) 
    delay(time);
  else if (time < 0){
    delay(-time);
    serialLCD.write(254); 
    serialLCD.write(128); 
    serialLCD.print("                ");
    serialLCD.write(254); 
    
    serialLCD.write(192);     
    serialLCD.print("                ");
    serialLCD.write(124); //turn of backlight
    serialLCD.write(128);
    delay(10);
    }
 
}

void enableGprsFONA808(char* apn,char* user,char* pwd){
    String ok_string;
  //messageLCD(2000,"AT+CGATT=1");
  if(true==fona.sendCheckReply(F("AT+CGATT=1"), F("OK"),10000))
  ok_string = "1";

  //messageLCD(1000,"AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  if(true==fona.sendCheckReply(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""), F("OK"),10000))
  ok_string += "2";

  char AT_string[50] = "AT+SAPBR=3,1,\"APN\",\"";
  strcat(AT_string, apn);
  strcat(AT_string, "\"");
  //messageLCD(1000,String(AT_string));
  if(true==fona.sendCheckReply(AT_string, "OK",10000))
  //if(true==fona.sendCheckReply("AT+SAPBR=3,1,\"APN\",\"online.telia.se\"", "OK",10000))
  ok_string += "3";

  strcpy(AT_string,"AT+SAPBR=3,1,\"USER\",\"");
  strcat(AT_string, user);
  strcat(AT_string, "\"");
  //messageLCD(1000,String(AT_string));
  if(true==fona.sendCheckReply(AT_string, "OK",10000))
  //if(true==fona.sendCheckReply("AT+SAPBR=3,1,\"USER\",\"\"", "OK",10000))
  ok_string += "4";
 
  strcpy(AT_string,"AT+SAPBR=3,1,\"PWD\",\"");
  strcat(AT_string, pwd);
  strcat(AT_string, "\"");
  //messageLCD(1000,String(AT_string));
  if(true==fona.sendCheckReply(AT_string, "OK",10000))
  //if(true==fona.sendCheckReply("AT+SAPBR=3,1,\"PWD\",\"\n", "OK",10000))
  ok_string += "5";
    
  //messageLCD(1000,"AT+SAPBR=1,1"); 
  
  if(true==fona.sendCheckReply(F("AT+SAPBR=1,1"), F("OK"),10000))
  ok_string += "6";
  //messageLCD(5000,ok_string);
}

void initFONA808(){     
  fonaSerial->begin(4800);
  while (! fona.begin(*fonaSerial)) {
    pinMode(FONA_POWER_KEY, OUTPUT);
    digitalWrite(FONA_POWER_KEY, HIGH);
    delay(100);   
    digitalWrite(FONA_POWER_KEY, LOW);
    delay(2000);
    digitalWrite(FONA_POWER_KEY, HIGH);
    delay(100);
  }

  fona.enableGPS(true);
  delay(1000);
  
  enableGprsFONA808("online.telia.se","","");
  //fona.setGPRSNetworkSettings(F("online.telia.se"));
  //fona.enableGPRS(true);
  float *lonGSM = 0;
  float *latGSM = 0;
 
  //fona.setGPRSNetworkSettings(F("online.telia.se"));
  //fona.enableGPRS(true);
  fona.enableNTPTimeSync(true, F("pool.ntp.org"));

}



int8_t readFONA808(float *laGPS, float *loGPS,float *laGSM, float *loGSM, boolean *mode, char *IMEInr, uint16_t *batt){

  
  // Grab the IMEI number
  uint8_t imeiLen = fona.getIMEI(IMEInr);
  
  // Grab GPS latitude/longitude
  if (boolean gps_success = fona.getGPS(laGPS, loGPS)) {
    *mode = !GSM_ONLY;
  }
  else
    *mode = GSM_ONLY;

  //Grab GSM latitude/longitude data.
  boolean gsmloc_success = fona.getGSMLoc(loGSM, laGSM);
  fona.getBattPercent(batt); 

  return fona.GPSstatus();
}

void getGPSposFONA808(char *latAVG_str, char *lonAVG_str, char *fix_qualityAVG_str, int samples){
  int i = samples;
  short counterAVG = 0;  

  char      str_fix[3];
  float     latGPS = 0;
  float     latGSM = 0;
  float     lonGPS = 0;
  float     lonGSM = 0;
  float     latAVG = 0;
  float     lonAVG = 0;
  float     fix_qualityAVG = 0;
    
  int8_t    fix_quality = 0;
  boolean   mode;
  fona.getBattPercent(&batteryLevel);
  messageLCD(2000,F("FONA col. data"),"Battery: " + String(batteryLevel)+ "%");
    
  do{    
    fix_quality = readFONA808(&latGPS, &lonGPS, &latGSM, &lonGSM, &mode, IMEI_id, &batteryLevel);
      
    if(fix_quality >= 2){
      fix_qualityAVG += fix_quality;
      latAVG += latGPS;
      lonAVG += lonGPS;
      counterAVG++;  
      dtostrf(latGPS, 9, 5, latAVG_str);
      dtostrf(lonGPS, 9, 5, lonAVG_str);
      messageLCD(0,String(latAVG_str) + " GPS " + String(batteryLevel),String(lonAVG_str) + " #" + String(counterAVG));         
    }
  else {
      fix_qualityAVG += fix_quality;
      latAVG += latGSM;
      lonAVG += lonGSM;
      counterAVG++;  
      dtostrf(latGSM, 9, 5, latAVG_str);
      dtostrf(lonGSM, 9, 5, lonAVG_str);
      messageLCD(0,String(latAVG_str) + " GSM " + String(batteryLevel),String(lonAVG_str) + " #" + String(counterAVG));         
    
      //messageLCD(0,"Fix: " + String(fix_quality), String(i--) + "/" + String(samples) + "  batt% " + String(batteryLevel));  
    }
    delay(2000); 
       
  } while(i>0 && counterAVG < samples );
      
  latAVG/=counterAVG;
  lonAVG/=counterAVG;
  Serial.print("latAVG=");
  Serial.println(String(latAVG));
  Serial.print("lonAVG=");
  Serial.println(String(lonAVG));
  fix_qualityAVG/=counterAVG;
  dtostrf(latAVG, 9, 5, latAVG_str);
  dtostrf(lonAVG, 9, 5, lonAVG_str);
  
    Serial.print("latAVG_str=");
  Serial.println(String(latAVG_str));
    Serial.print("lonAVG_str=");
  Serial.println(String(lonAVG_str));
  /*
  if (mode == GSM_ONLY){
    dtostrf(latGSM, 9, 5, str_lat);
    dtostrf(lonGSM, 9, 5, str_lon);
    messageLCD(3000,F("Pos method:"),F("GSM ONLY"));
    messageLCD(8000,String(str_lat) + " GSM",String(str_lon));
  }
  else { 
    messageLCD(3000,F("Pos method"), F("GPS"));
    //dtostrf(*latAVG, 9, 5, str_lat);
    //dtostrf(*lonAVG, 9, 5, str_lon);
    //dtostrf(*fix_qualityAVG, 4, 1, str_fix);
    messageLCD(8000,String(latAVG_str) + " GPS", String(lonAVG_str) + ": "+ fix_qualityAVG_str);
    
  } */    
}

void closeFONA808(){
  pinMode(FONA_POWER_KEY, OUTPUT);
  FONA_POWER_KEY == HIGH;
  delay(500);   
  digitalWrite(FONA_POWER_KEY, LOW);
  delay(2000);
  digitalWrite(FONA_POWER_KEY, HIGH);
  pinMode(FONA_POWER_KEY, OUTPUT);
  delay(500);
}


int sendDataServer(char* url, char *data){
  
  data[dataIndex++]='#'; 
  data[dataIndex++]='1'; 
  data[dataIndex++]='#'; 

  unsigned short crc;
  //crcsum((const unsigned char*)data,dataIndex,crc);
  char buf [6];
  sprintf (buf, "%06i", crc);; 
Serial.println(crc);
  
  uint16_t statuscode;
  int16_t length;

  //String(buffer).substring(1,22).toCharArray(date,21);
  //sprintf(data2,"latitude=%s&longitude=%s&time=%s&mode=%s",str_lat,str_lon,date,gps_mode);

  fona.HTTP_POST_start(url, F("application/x-www-form-urlencoded"), (uint8_t *) data, strlen(data), &statuscode, (uint16_t *)&length);
  
  
  Serial.print("The HTTP POST status was:");
  Serial.println(statuscode);
  Serial.println(data);
  return statuscode;
}



//---------------------------------------
/* SKRIV TILL FLASH MINNE
char EEMEM eepromString[10]; //declare the flsah memory.
    
    while (!eeprom_is_ready());
    char save[] ="bananpaj";
    cli();    //disable interupts so it's not disturbed during write/read
    eeprom_write_block(save, &eepromString[5],sizeof(save));
     while (!eeprom_is_ready());
     char ramString[10];
     eeprom_read_block( &ramString[0], &eepromString[5], sizeof(save));
      sei(); //enable itnerupts again.
     Serial.println(ramString);
     */
//---------------------------------------


//SETUP
//-------------------------------------------------------------------------------------------
void setup() {
  serialLCD.begin(9600);
  delay(500);
  Serial.begin(115200);
  messageLCD(0,F("booting."));
  /*
  Serial.print("hex=");
  unsigned short crc=0xFFFF; 
  //"123456789".getbytes(message,9)
  unsigned char* message =(unsigned char*) "123456789";
  Serial.println(crcsum(message, (unsigned long) 9, crc),HEX);
Serial.println(crcsum(message, (unsigned long) 9, crc));
 //crc16_update((uint16_t) 0xffff, (uint8_t)"1234567890");
  //Serial.println(crc16((unsigned char*)"1234567890", 10),HEX);
  */
  pinMode(FONA_POWER_KEY, OUTPUT);
  digitalWrite(FONA_POWER_KEY, HIGH);
  
  // This next section of code is timing critical, so interrupts are disabled.
  noInterrupts();
  
  // Set the watchdog reset bit in the MCU status register to 0.
  MCUSR &= ~(1<<WDRF);
  
  // Set WDCE and WDE bits in the watchdog control register.
  WDTCSR |= (1<<WDCE) | (1<<WDE);

  // Set watchdog clock prescaler bits to a value of 8 seconds.
  WDTCSR = (1<<WDP0) | (1<<WDP3);
  
  // Enable watchdog as interrupt only (no reset).
  WDTCSR |= (1<<WDIE);
  
  // Enable interrupts again.
  interrupts();  

}

//LOOP
//-------------------------------------------------------------------------------------------
void loop() {
  // Don't do anything unless the watchdog timer interrupt has fired.
  if (watchdogActivated){
      watchdogActivated = false;
      
    // Increase the count of sleep iterations and take a sensor
    // reading once the max number of iterations has been hit.
    sleepIterations += 1;
    if (sleepIterations >= MAX_SLEEP_ITERATIONS_GPS) {
      
      messageLCD(0,F("Awake!"));// + (int) 8*sleepIterations);
      // Reset the number of sleep iterations.
      sleepIterations = 0;

      //DO SOME WORK!
      //Fire up FONA 808 GPS and take a position reading.
      messageLCD(0,F("FONA power up"));
      initFONA808();

      
      char latAVG_str[12] = "0";
      char lonAVG_str[12] = "0";
      char fix_qualityAVG_str[5] = "0";


      
      getGPSposFONA808(latAVG_str, lonAVG_str, fix_qualityAVG_str,3);


      Serial.println("");
      Serial.println(dataIndex);
      Serial.println("lat=");
      for(char i=0;i<9;i++){
        data[dataIndex++]=latAVG_str[i]; 
        Serial.print(latAVG_str[i]);
        }
      Serial.println("");
      Serial.println(dataIndex);
      data[dataIndex++]='#';

      Serial.println("lon=");
      for(char i=0;i<9;i++){
        data[dataIndex++]=lonAVG_str[i]; 
        Serial.print(lonAVG_str[i]);
      }    
      data[dataIndex++]='#';
      Serial.println("");
      Serial.println(dataIndex);
      char dateAndTime[23];
      fona.getTime(dateAndTime,23);

      data[dataIndex++]=dateAndTime[1];
      data[dataIndex++]=dateAndTime[2];
      data[dataIndex++]=dateAndTime[4];
      data[dataIndex++]=dateAndTime[5];
      data[dataIndex++]=dateAndTime[7];
      data[dataIndex++]=dateAndTime[8];
      data[dataIndex++]='#';    
      data[dataIndex++]=dateAndTime[10];
      data[dataIndex++]=dateAndTime[11];
      data[dataIndex++]=dateAndTime[13];
      data[dataIndex++]=dateAndTime[14];
      data[dataIndex++]=dateAndTime[16];
      data[dataIndex++]=dateAndTime[17];    
      data[dataIndex++]='#';                        
     // for(short i=1;i<9;i++)
      //  data[dataIndex++]=dateAndTime[i];
      //  data[dataIndex++]=  
      
     // data[dataIndex++]='#'; 
     // for(short i=10;i<18;i++)          
     //   data[dataIndex++]=dateAndTime[i]; 
        
      //data[dataIndex++]='#';     
      data[dataIndex]='1';
      dataIndex+=1;
      data[dataIndex++]='#';     
      data[dataIndex]='2';
      dataIndex+=1;
      dataCounter++;
      Serial.print("data0=");
      Serial.println(data);
      
      messageLCD(2000, F("FONA shutdown"));
      closeFONA808();

      messageLCD(-2000, F("Zzzz"));
      dataCounter++;

    }
    if(dataCounter % 3 ==0) {
      if(true == sendDataServer("http://pi1.lab.hummelgard.com:88/addData", data));
      dataIndex=0;
    }
   
  }
  // Go to sleep!
  sleep();
  
}
