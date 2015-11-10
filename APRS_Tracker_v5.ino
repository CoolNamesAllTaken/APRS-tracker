#define SERIAL_BAUD 9600
#define GPS_BAUD 9600
#define SENSOR_BAUD 9600
#define SD_BAUD 9600
#define LOOP_DELAY 20000

//APRS Message Constants
const char MESSAGE_STARTER[12] = ":KK6NQK-9 :";
const char MESSAGE_ENDER[3] = "{1";

//APRS Position Update Constants
const int APRS_SENTENCE_MAX = 120;
const char POSITION_STARTER[2] = "@";  //indicates mobile station with messaging capabilities
const char SYMBOL_CODE_1[2] = "/";  //APRS symbol table 1 index
const char SYMBOL_CODE_2[2] = "O";  //APRS symbol table 2 index
const char TIME_ENDER[2] = "z";

//GPS Constants
const int GPS_SENTENCE_MAX = 80;  //max length of GPS sentence
const int GPS_FIELD_MAX = 80;  //max length of a GPS field
const int VALID_GPS_SENTENCE_LENGTH = 75;  //max length of a GPS sentence that is considered "valid"--prevents run-on garbage serial data sentences

//GPS sentences
char gpsSentence[GPS_SENTENCE_MAX];  //char array that stores current GPS sentence
char lastValidGPSSentence[GPS_SENTENCE_MAX] = "lastValidGPSSentence Init";

//GPS fields
char time[15] = "hhmmss";
char latitude[15] = "lat ini";  //llll.lla
char longitude[15] = "long ini";  //yyyyy.yya
char fix[15] = "fini";  //xxxx
char altitude[15] = "alt ini"; //xxxxx.xM
char packetNumber[9] = "00000000";

//sensor stuff
const int SENSOR_SENTENCE_MAX = 50;  //max length of sensor data sentence
char sensorSentence[SENSOR_SENTENCE_MAX] = "sensorSentence Init";

//APRS stuff
int pNum = 0;  //packet number as an integer
boolean txGPS = false;  //boolean that determines whether to transmit GPS or sensor data
char APRSSentence[APRS_SENTENCE_MAX] = "APRSSentence init";

/**
*  Setup method that runs first time through, initializes serial inputs / outputs
*  NOTE: This code intended for Arduino Mega 2560 in order to make use of multiple serial ports
**/
void setup()
{
  Serial.begin(SERIAL_BAUD);  //initialize main serial port
  Serial1.begin(GPS_BAUD);  //initialize GPS serial port
  Serial2.begin(SENSOR_BAUD);  //initialize sensor data port
  Serial3.begin(SD_BAUD);  //initialize SD card data port
}

/**
*  Main loop that runs continuuously while sketch is loaded.  Alternates between transmitting sensor data as APRS message
*  and transmitting GPS data as position update.  Loops every 20 seconds and keeps track of packet number as it runs.
**/
void loop()
{ 
  checkGPS();  //update GPS sentence if GPS data available
  convertGPSSentence(lastValidGPSSentence);  //fill in GPS fields with last valid GPS sentence
  checkSensors();  //update sensor sentence if sensor data available
  
  APRSSentence[0] = (char)0;  //clear APRS sentence
 
  if(txGPS == false)  //transmit message
  {
    //NOTE: only transmits first 59 characters of data sentence due to APRS message line restrictions
    //need to parse data into multiple lines, change message ender to message line # for multi-line data transmission
    strcat(APRSSentence, MESSAGE_STARTER);
    strcat(APRSSentence, "P=");
    strcat(APRSSentence, packetNumber);
    strcat(APRSSentence, "/Data=");
    strcat(APRSSentence, sensorSentence);
    strcat(APRSSentence, MESSAGE_ENDER);
  }
  else  //transmit position report
  {
    strcat(APRSSentence, POSITION_STARTER);
    strcat(APRSSentence, time);
    strcat(APRSSentence, TIME_ENDER);
    strcat(APRSSentence, latitude);
    strcat(APRSSentence, SYMBOL_CODE_1);
    strcat(APRSSentence, longitude);
    strcat(APRSSentence, SYMBOL_CODE_2);
    strcat(APRSSentence, "P=");
    strcat(APRSSentence, packetNumber);
    strcat(APRSSentence, "/F=");
    strcat(APRSSentence, fix);
    strcat(APRSSentence, "/A=");
    strcat(APRSSentence, altitude);
  }
  
  Serial.println(APRSSentence);
  Serial3.println(gpsSentence);
  
  pNum ++;  //increment pNum
  snprintf(packetNumber, sizeof packetNumber, "%d", pNum);  //store packetNumber 
  txGPS = !txGPS;
 
  delay(LOOP_DELAY);  //delay before re-starting loop
  
  Serial.flush();  //flush APRS data
  Serial1.flush();  //flush GPS data
  Serial2.flush();  //flush sensor data
  Serial3.flush();
}

/**
*  Checks sensor port to see if there is data, reads in data if possible and stores in sensorSentence
*  return: 1 if data found and stored, 0 if no data
**/
int checkSensors()
{
   if(Serial2.available()) //is sensor data available?
  {
    int i = 0;
    boolean reading = false;
    while(Serial2.available()) //read serial data until gone
    { 
      char ch = Serial2.read();  //read char from sensor serial
      if(reading == false && ch == '$') //start reading at first '$' character
      {
        i = 0;
        reading == true;
        
        sensorSentence[i] = ch; //put first char into sentence
        i++;
      }
      else if (ch != '$' && i < SENSOR_SENTENCE_MAX) //read until hit next '$' or sentence size
      {
        sensorSentence[i] = ch; //put char into sentence
        i++;
      }
    }
       sensorSentence[i] = '\0'; //put end-of-line char into sentence
    
    return 1; //sensor read successful
  }
  else
  {
    return 0; //sensor read didn't find data
  }
}

/**
*  Checks GPS serial port to see if there is data, reads in data if possible and stores in gpsSentence.
*  return: 1 if data found and stored, 0 if no data.
**/
int checkGPS()
{
  if(Serial1.available()) //is GPS data available?
  {
    int i = 0;
    boolean reading = false;
    while(Serial1.available()) //read serial data until gone
    { 
      char ch = Serial1.read();  //read char from GPS serial
      
      if(reading == false && ch == '$') //start reading at first '$' character
      {
        i = 0;
        reading == true;
        
        gpsSentence[i] = ch; //put first char into sentence
        i++;
      }
      else if (ch != '$' && i < GPS_SENTENCE_MAX) //read until hit next '$' or sentence size
      {
        gpsSentence[i] = ch; //put char into sentence
        i++;
      }
    }
       gpsSentence[i] = '\0'; //put end-of-line char into sentence
       
       char field[GPS_FIELD_MAX];
       getField(gpsSentence, field, 0);
       if(strcmp(field,"$GPGGA") == 0 && i <= VALID_GPS_SENTENCE_LENGTH)  //GPS sentence valid GPGGA sentence?
       {
         memcpy(lastValidGPSSentence, gpsSentence, i-1);  //store GPS sentence as last valid GPS sentence
       }
    
    return 1; //GPS read successful
  }
  else
  {
    return 0; // GPS read didn't find data
  }
}

/**
*  Converts GPS sentence into user-readable lat/long format.
*  Recognized formats: GPGGA
*  return: user-readable string derived from current gpsSentence
**/
int convertGPSSentence(char* gpsSentenceIn)
{
  char field[GPS_FIELD_MAX];  //maximum length of GPS field
  getField(gpsSentenceIn, field, 0);  //fill field with first GPS field
  
if(strcmp(field, "$GPGGA") == 0) //GPS data in GPGGA format?
 {
    //convert GPS sentence to APRS format
    //GPS time  "hhmmss"
    time[0] = (char)0;
    getField(gpsSentenceIn, field, 1);  //GPS time field
    strcat(time, field);
    time[6] = (char)0;  //shorten to 6 digits
    
    //GPS Latitude  "llll.ll"
    latitude[0] = (char)0;
    getField(gpsSentenceIn, field, 2);  //GPS Latitude number field
    strcat(latitude, field);  //Lat:llll.ll
    latitude[7]=(char)0;  //shorten to two decimals
    getField(gpsSentenceIn, field, 3);  //GPS Latitude N/S field
    strcat(latitude, field);  //a
    
    //GPS Long  "yyyyy.yy"
    longitude[0] = (char)0;
    getField(gpsSentenceIn, field, 4);  //GPS Longitude number field
    strcat(longitude, field);  //yyyyy.yy
    longitude[8]=(char)0;  //shorten to two decimals
    getField(gpsSentenceIn, field, 5);  //GPS Longitude E/W field
    strcat(longitude, field);  //a
    
    
    //GPS fix  "xxxx"
    fix[0] = (char)0;
    getField(gpsSentenceIn, field, 6);  //GPS fix status field
    if(field[0] == '0')  //invalid?
    {
      strcat(fix, "NO");
    }
    else if(field[0] == '1')  //GPS fix?
    {
      strcat(fix, "OK");
    }
    else  //read failure?
    {
      strcat(fix, "??");
    }
    getField(gpsSentenceIn, field, 7);  //number of satellites field
    strcat(fix, field);  //xx
    
    //GPS altitude above sea level  "xxxxx.x"
    altitude[0] = (char)0;
    getField(gpsSentenceIn, field, 9);  //GPS altitude number field
    strcat(altitude, field);
  }
  else  //GPS data format not recognized
  {
    //ERROR HANDLING
    return 0;  //return 0 if conversion failed
  }
  
  return 1;  //return 1 if conversion worked
}

/**
*  Takes pointer to a character array and index of commacount in gpsSentence and fills character
*  array with characters in specified gpsSentence field
*  char* buffer: pointer to beginning of char array to fill
*  int index: comma count of field to parse
**/
void getField(char* gpsSentenceIn, char* buffer, int index)
{
  int gpsSentencePos = 0;
  int fieldPos = 0;
  int commaCount = 0;
  
  while (gpsSentencePos < GPS_SENTENCE_MAX) //keep going while within GPS sentence
  {
    if (gpsSentenceIn[gpsSentencePos] == ',')  //hit a comma?
    {
      commaCount ++;
      gpsSentencePos ++;
    }
    if (commaCount == index)  //within target field?
    {
      buffer[fieldPos] = gpsSentenceIn[gpsSentencePos];  //copy char in target field char to buffer
      fieldPos ++;
    }
    gpsSentencePos ++;
  }
  buffer[fieldPos] = '\0';  //end field with end-of-string character
} 
