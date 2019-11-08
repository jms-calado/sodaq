#include <Sodaq_R4X.h>
#include <Arduino.h>
#include <Wire.h>
#include <Sodaq_UBlox_GPS.h>
#include <ArduinoJson.h>

#define MySerial        SERIAL_PORT_MONITOR
#define ARRAY_DIM(arr)  (sizeof(arr) / sizeof(arr[0]))

#define CONSOLE_STREAM   SerialUSB
#define MODEM_STREAM     Serial1
#define DEBUG_STREAM  SerialUSB

#define CURRENT_APN      "ep.inetd.gdsp" //"swisscom-test.m2m.ch" for Swisscom CH; "internetm2m" for Altice PT; "ep.inetd.gdsp" for Vodafone IE
#define CURRENT_URAT     "8" //NB-IOT
#define MQTT_SERVER_IP   "87.44.18.29" //mqtt.staging.carelink.tssg.org
#define MQTT_SERVER_PORT 1883
#define MQTT_USERNAME    "username"
#define MQTT_KEY         "password"

String deviceId = "PS99";

static Sodaq_R4X r4x;
static Sodaq_SARA_R4XX_OnOff saraR4xxOnOff;

static bool isReady = false;
static bool MQTTisReady;
bool isSubscribed;
bool isActive = false;  

#define publishCycle 100U
#define subscribeCycle 275U

unsigned long publishLastMillis = 0;
unsigned long subscribeLastMillis = 0;

boolean publishState = false;
boolean subscribeState = false;

#define ADC_AREF 3.3f
#define BATVOLT_R1 4.7f // SFF, AFF, One v2 and v3
#define BATVOLT_R2 10.0f // SFF, AFF, One v2 and v3
#define BATVOLT_PIN BAT_VOLT

StaticJsonDocument<200> doc;

void find_fix(uint32_t delay_until);
void do_flash_led(int pin);

/*******************************************************************************************    SETUP    *****************************************************************/

void setup() {
  
  CONSOLE_STREAM.println("Booting up...");

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);


  MODEM_STREAM.begin(r4x.getDefaultBaudrate());
  r4x.setDiag(CONSOLE_STREAM);
  r4x.init(&saraR4xxOnOff, MODEM_STREAM);
  delay(1000);
  sodaq_gps.init(GPS_ENABLE);
  do_flash_led(LED_RED);
  CONSOLE_STREAM.println("Waiting for NB connection...");
  isReady = r4x.connect(CURRENT_APN, CURRENT_URAT, "524288");//ultimo parâmetro é o bandmask, que é configuravel(B20->"524288", B8->"128", B7->"64", B3->"4")
  CONSOLE_STREAM.println(isReady ? "Network connected" : "Network connection failed");//fazer tratamento dos erros
  
  if (isReady) {
      do_flash_led(LED_GREEN);
      MQTTisReady = r4x.mqttSetServerIP(MQTT_SERVER_IP, MQTT_SERVER_PORT) && r4x.mqttSetAuth(MQTT_USERNAME, MQTT_KEY) && r4x.mqttLogin();//r4x.mqttSetAuth(MQTT_USERNAME, MQTT_KEY)
      CONSOLE_STREAM.println(MQTTisReady ? "MQTT connected" : "MQTT failed");
   }

   if (MQTTisReady) {
      do_flash_led(LED_GREEN);
      String topic = deviceId + "/active";
      bool isSubscribed = r4x.mqttSubscribe(topic.c_str());
      CONSOLE_STREAM.println(isSubscribed ? "Subscribed to active" : "Fail to subscribe");
      delay(1000);
   }
  
    String topic = deviceId + "/configuration/energyProfile";
    bool response = r4x.mqttSubscribe(topic.c_str());
    CONSOLE_STREAM.println(response ? "Subscribed to energyProfile" : "Fail to subscribe");
}

/*******************************************************************************************    LOOP    *****************************************************************/

void loop() {

  if(!isReady) {
    CONSOLE_STREAM.println("Waiting for NB connection...");
    isReady = r4x.connect(CURRENT_APN, CURRENT_URAT, "524288");//ultimo parâmetro é o bandmask, que é configuravel(B20->"524288", B8->"128", B7->"64", B3->"4")
    CONSOLE_STREAM.println(isReady ? "Network connected" : "Network connection failed");//fazer tratamento dos erros
  }
  else {
    CONSOLE_STREAM.println(isActive ? "Funcionamento normal" : "Ainda não está active");//fazer tratamento dos erros
    if(isActive) {
      if(cycleCheck(&publishLastMillis, publishCycle)) {
         do_flash_led(LED_BLUE);
         find_fix(5000);
         publishState = !publishState;
      }
      if(cycleCheck(&subscribeLastMillis, subscribeCycle)) {
         do_flash_led(LED_GREEN);
         receiveSubscribedTopicsMessages();
         subscribeState = !subscribeState;
      }
      //receiveSubscribedTopicsMessages();
      //do_flash_led(LED_BLUE);
      //find_fix(5000);
    }
    else {
      delay(5000);
      do_flash_led(LED_GREEN);
      receiveSubscribedTopicsMessages();
    }
  }
  delay(1000);
}

/***************************************************************************************    MAIN FUNCTIONS    ***********************************************************/

void receiveSubscribedTopicsMessages() {
    r4x.mqttLoop();
    CONSOLE_STREAM.println("Wait for subscribed messages");
    if (r4x.mqttGetPendingMessages() > 0) {
        char buffer[2048];
        uint16_t numMessages = r4x.mqttReadMessages(buffer, sizeof(buffer));
        
        CONSOLE_STREAM.print("Read messages:");
        CONSOLE_STREAM.println(numMessages);
        //CONSOLE_STREAM.println(buffer);
        if(numMessages == 1) {
         CONSOLE_STREAM.print("message:");
          String str = strtok(buffer, "\r\n");
          CONSOLE_STREAM.println(str);
 
         // int index = str-buffer;
          CONSOLE_STREAM.println(str.length());
          int t = str.length();//11
          char topic[t];
          strcpy(topic, str.c_str());
          CONSOLE_STREAM.print("topic:");
          CONSOLE_STREAM.println(topic);
          str = strtok(NULL, "\n");
          char message[str.length()];
          strcpy(message, str.c_str());
          CONSOLE_STREAM.print("message:");
          CONSOLE_STREAM.println(message);
          processSubscribeMessage(topic, message, true);
        } 
        else {
          String str = strtok(buffer, "\n");
          if( str !=NULL) {
            for(int nVezes = 0; nVezes < numMessages; nVezes++) {
              char topic[str.length()-1];
              for (int i = 0; i < str.length()-1; i++)
              {
                topic[i] = str[i];
              }
              CONSOLE_STREAM.print("topic:");
              CONSOLE_STREAM.println(topic);
              str = strtok(NULL, "\n");
              char message[str.length()];
              strcpy(message, str.c_str());
              CONSOLE_STREAM.print("message:");
              CONSOLE_STREAM.println(message);
              processSubscribeMessage(topic, message, false);
              str = strtok(NULL, "\n");
            }
          }
        }
    }
  delay(5000);
}

bool publishLocation(String input) {
  uint8_t locationMessage[input.length()];
  char char_array[input.length()];
  strcpy(char_array, input.c_str());
  for (int i = 0; i < strlen(char_array); i++)
  {
    locationMessage[i] = uint8_t(char_array[i]);
  }
  CONSOLE_STREAM.println(char_array);
  String topic = deviceId + "/status";
  bool response = r4x.mqttPublish(topic.c_str(), locationMessage, sizeof(locationMessage), 0, 0, 1);
  return response;
}

void find_fix(uint32_t delay_until) {
    CONSOLE_STREAM.println(String("delay ... ") + delay_until + String("ms"));
    delay(delay_until);

    uint32_t start = millis();
    uint32_t timeout = 30000;
    CONSOLE_STREAM.println(String("waiting for fix ..., timeout=") + timeout + String("ms"));
    if (sodaq_gps.scan(false, timeout)) {
       do_flash_led(LED_GREEN);
       CONSOLE_STREAM.println(String(" time to find fix: ") + (millis() - start) + String("ms"));
       CONSOLE_STREAM.println(String(" datetime = ") + sodaq_gps.getDateTimeString());
       CONSOLE_STREAM.println(String(" lat = ") + String(sodaq_gps.getLat(), 7));
       CONSOLE_STREAM.println(String(" lon = ") + String(sodaq_gps.getLon(), 7));
       CONSOLE_STREAM.println(String(" alt = ") + String(sodaq_gps.getAlt(), 7));
       CONSOLE_STREAM.println(String(" num sats = ") + String(sodaq_gps.getNumberOfSatellites()));
       String input = "{\"timestamp\":\"" + formatDateTime(sodaq_gps.getYear(), sodaq_gps.getMonth(), sodaq_gps.getDay(), sodaq_gps.getHour(), sodaq_gps.getMinute(), sodaq_gps.getSecond()) + "\",\"location\": {\"lat\":" + String(sodaq_gps.getLat(), 7) + ",\"lon\":" + String(sodaq_gps.getLon(), 7) + ",\"alt\":" + String(sodaq_gps.getAlt(), 7) + ",\"batteryLevel\":" + String(getBatteryVoltage() * 0.001, 3) + "}}";
       isActive = publishLocation(input);
       
    } else {
       CONSOLE_STREAM.println("No Fix");
       String input = "{\"timestamp\":\"" +  formatDateTime(1970, 01, 01, 00, 00, 00) + "\",\"location\": {\"lat\":" +  0 + ",\"lon\":" + 0 + ",\"alt\":" +  0 + ",\"batteryLevel\":" +  String(getBatteryVoltage() * 0.001, 3) + "}}";
       isActive = publishLocation(input);
       do_flash_led(LED_RED);
    }
}

boolean cycleCheck(unsigned long *lastMillis, unsigned int cycle) 
{
 unsigned long currentMillis = millis();
 if(currentMillis - *lastMillis >= cycle)
 {
   *lastMillis = currentMillis;
   return true;
 }
 else
   return false;
}

int getBatteryVoltage()
{
    uint16_t voltage = (uint16_t)((ADC_AREF / 1.023) * (BATVOLT_R1 + BATVOLT_R2) / BATVOLT_R2 * (float)analogRead(BATVOLT_PIN));
    //int batteryPercentage;
    CONSOLE_STREAM.println(voltage);
    return (int)voltage;
    /*if(voltage<=3200) {
       CONSOLE_STREAM.println("Battery discharged");
       batteryPercentage = 0;
    }
    else if(voltage <= 3400){
      CONSOLE_STREAM.println("Battery level 10%");
      batteryPercentage = 10;
    }
    else if(voltage <= 3500) {
      CONSOLE_STREAM.println("Battery level 20%");
      batteryPercentage = 20;
    } 
    else if(voltage <= 3600) {
      CONSOLE_STREAM.println("Battery level 30%");
      batteryPercentage = 30;
    }
    else if(voltage <= 3700) {
      CONSOLE_STREAM.println("Battery level 40%");
      batteryPercentage = 40;
    }
    else if(voltage <= 3800) {
      CONSOLE_STREAM.println("Battery level 50%");
      batteryPercentage = 50;
    }
    else if(voltage <= 3900) {
      CONSOLE_STREAM.println("Battery level 60%");
      batteryPercentage = 60;
    }
    else if(voltage <= 4000) {
      CONSOLE_STREAM.println("Battery level 80%");
      batteryPercentage = 80;
    }
    else {
      CONSOLE_STREAM.println("Battery level 100%");
      batteryPercentage = 100;
    }*/
}

/*****************************************************************************************   AUX FUNCTIONS   *****************************************************************************/

uint8_t* formatPublishMessage(String input) {
    uint8_t publishMessage[input.length()];
    char char_array[input.length()]; 
    strcpy(char_array, input.c_str());
    for (int i = 0; i < strlen(char_array); i++)
    {
      publishMessage[i] = uint8_t(char_array[i]);
    }
    CONSOLE_STREAM.println(char_array);
    return publishMessage;
}

/* {"gnss":{"active":true,"sr":"5000"},"lteNB":{"active":true,"sr":"10000"}} length=73 */
void processSubscribeMessage(char topic[], char buffer[], boolean isNormal) {
    
    String topicAux = deviceId + "/active";
    
    if(strcmp(topic,topicAux.c_str()) == 0) {
        CONSOLE_STREAM.println("Found message of topic /active");
        //CONSOLE_STREAM.println(buffer);
        //CONSOLE_STREAM.println(strlen(buffer)-2);
        if(isNormal) {
          char message[5] = {buffer[0], buffer[1], buffer[2], buffer[3], 0};
          CONSOLE_STREAM.println(message);
          isActive = (strcmp(message,"true") == 0);
        }
        else {
          char message[5] = {buffer[0], buffer[1], buffer[2], buffer[3], 0};
          CONSOLE_STREAM.println(message);
          isActive = (strcmp(message,"true") == 0);
        }
        //CONSOLE_STREAM.print(strcmp(message,"true"));
        CONSOLE_STREAM.print("Values:");
        CONSOLE_STREAM.println(isActive);
    }
    else {
      CONSOLE_STREAM.println("Found message of topic /energyProfile");
      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, buffer);
      // Test if parsing succeeds.
      if (error) {
        CONSOLE_STREAM.println(F("deserializeJson() failed: "));
        CONSOLE_STREAM.println(error.c_str());
        return;
      }
      boolean gnssActive = doc["gnss"]["active"].as<boolean>();
      long gnssTime = doc["gnss"]["sr"].as<long>();
      boolean nbActive = doc["lteNB"]["active"].as<boolean>();
      long nbTime = doc["lteNB"]["sr"].as<long>();
      CONSOLE_STREAM.print("Values:");
      CONSOLE_STREAM.println(gnssActive);
      CONSOLE_STREAM.println(gnssTime);
      CONSOLE_STREAM.println(nbActive);
      CONSOLE_STREAM.println(nbTime);
    }
}

String formatDateTime(int year, int month, int day, int hour, int minute, int second) {
    String datetime = "";
    datetime += num2String(year,4) + "-" + num2String(month, 2) + "-" + num2String(day,2) + "T" + num2String(hour,2) + ":" + num2String(minute,2) + ":" + num2String(second,2) + "Z";
    CONSOLE_STREAM.println("date: " + datetime);
    return datetime;
}

void do_flash_led(int pin)
{
    for (size_t i = 0; i < 2; ++i) {
        delay(1000);
        digitalWrite(pin, LOW);
        delay(1000);
        digitalWrite(pin, HIGH);
    }
}

String num2String(int num, size_t width)
{
    String out;
    out = num;
    while (out.length() < width) {
        out = String("0") + out;
    }
    return out;
}
