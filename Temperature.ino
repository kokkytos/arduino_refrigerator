//Some general notes:
//
//pushingbox POST request στα scenarios
// API URL https://sms.liveall.eu/apiext/Sendout/SendSMS
//apitoken=3507c2e8fdd319ef15ad264549284853404aa7773151e1dcbb0c053e2bf6434e&destination=306993920210&senderid=Arduino&message=Θερμοκρασία: $temperature$ - Υγρασία: $humidity$ - Πόρτα: $door$
//
//WIFI and Temperature Circuit:http://www.ardumotive.com/iot-wifi-temp-and-humidity.html
//Screen  from Arduino Book
/**********************   include libraries   *************************/
#include <LiquidCrystal.h>
#include "DHT.h"
#include <SoftwareSerial.h>

const byte rxPin = 2; // Wire this to Tx Pin of ESP8266
const byte txPin = 3; // Wire this to Rx Pin of ESP8266 

// We'll use a software serial interface to connect to ESP8266
SoftwareSerial ESP8266 (rxPin, txPin);


/**********************   My variables   *************************/

#define DHTPIN 5     // what pin we're connect DHTPIN
#define DHTTYPE DHT22   // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE);

long a;
float temperature;
float humidity;

int errorcounter = 0;
int doorOpenCounter = 0;
int doorOpenCounterLimit = 5;
int errorcounterLimit = 5;


boolean initState = true;

float lowTemp = -1; // κάτω όριο σφάλματος θερμοκρασίας
float highTemp = 25; // άνω όριο σφάλματος θερμοκρασίας

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(4, 6, 10, 11, 12, 13);
unsigned long time;
long previousMillis = 0;        // will store last time posting was updated
const int ledBuzzer = 8;//the number of the buzzer


// the follow variables is a long because the time, measured in miliseconds,will quickly become a bigger number than can be stored in an int.
long interval = 300000; // interval at which to post data to web(milliseconds), every 5'
long previousTime1 = 0;
long interval1 = 1000; //main delay ever 1 sec

long previousTime2 = 0;
long interval2 = 10000; //screen delay ever 10 secs

long previousTime3 = 0;
long interval3 = 120000; //check if door is open every 2 minutes (maybe is better 10)


boolean sentSMS = false;
boolean doorOpen = false;
boolean initialPost = true;// use it for initial commit to thingspeak during Arduino startup so as avoid the delay (see 'interval' variable)
int error;

//*----------------- ThingSpeak settings -------------------*/
String TSWriteKey = "mykey"; //ThingSpeak Write Key, change it with your key...
#define IP "184.106.153.149"// thingspeak.com ip


/*-----------------ESP8266 Serial WiFi Module---------------*/
#define SSID "OTE2E40D4"     // 
#define PASS "mypass"       //
/*-----------------------------------------------------------*/



// constants won't change. They're used here to
// set pin numbers:
const int buttonPin = 7;     // the number of the pushbutton pin

// variables will change:


/*****************   SETUP   *******************************************/
void setup() {
  pinMode(buttonPin, INPUT);
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  dht.begin();
  pinMode(ledBuzzer, OUTPUT); //initialize the buzzer pin as output
  Serial.begin(9600);
  ESP8266.begin(9600); // Change this to the baudrate used by ESP8266
  ESP8266.println("AT");
  delay(5000);
  if (ESP8266.find("OK")) {
    connectWiFi();
  }
}


/*****************   Main Loop   *******************************************/

void loop() {
  
//Serial.println("Start Sound");
//digitalWrite(ledBuzzer, HIGH); //turn on the buzzer
//tone(ledBuzzer, 100);
//delay(10000);
//Serial.println("End Sound");
//digitalWrite(ledBuzzer, LOW); //turn on the buzzer
//noTone(ledBuzzer);



start: //label
  error = 0;
  if (initState == true) {
    lcd.clear();
    lcd.setCursor(0, 0); // lcd.setCursor(column, line);
    lcd.print("Welcome!");
    lcd.setCursor(0, 1);
    lcd.print("Please wait...");
    initState = false;
  }
  unsigned long currentTime1 = millis();
  if (currentTime1 - previousTime1 > interval1) {
    previousTime1 = currentTime1;

    
    //if door is closed, stop buzzer
    if (digitalRead(buttonPin) == LOW) {
            Serial.println("Door Is Closed");
            noTone(ledBuzzer);      
    }


    /***********************************/
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();// Read temperature as Celsius
    /***********************************/

    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    Serial.println("Temperature: " + String(temperature));
    Serial.println("Humidity: " + String(humidity));
    Serial.println("Door: " + String(digitalRead(buttonPin)));
    Serial.println("------------------------------------");



    //============== CHECK for errors: check every X minutes to see if door is open or temperature errors ============================
    unsigned long currentTime3 = millis();
    if (currentTime3 - previousTime3 > interval3) {
      previousTime3 = currentTime3;

      //*=====================  TEMPERATURE ERRORS ==========================*/
      // Ιn case of a temperature error
      if (checkTempError(temperature) == true ) {
        ++errorcounter;
      }
      else {
        errorcounter = 0;
        //noTone(ledBuzzer);
      }

      //Serial.println( "Temperature counter:"+String(errorcounter));

      //After x temperature errors in check loops, trigger alarm
      if (errorcounter >= errorcounterLimit) {
        //trigger alarm
        Serial.println("We have a temperature error. Alarm will be trigged");
        TemperatureAlarm(false);//trigger alarm, with SMS. to-do: set it to true during production release
        errorcounter = 0;//set to 0 so as to trigger it again after x errorcounter
      }


      //*=====================  DOOR CHECK ==========================*/
      Serial.println("Checking if door is open...");
      if (digitalRead(buttonPin) == HIGH) {
        ++doorOpenCounter;
        Serial.println("Door Is Open");
      }
      else {
        doorOpenCounter = 0;
        Serial.println("Door Is Closed");
        noTone(ledBuzzer);
        //digitalWrite(ledBuzzer, LOW); //turn off the buzzer        
      }
      // In case the door is Open for x time, trigger alarm
      if (doorOpenCounter > doorOpenCounterLimit ) {
        TemperatureAlarm(false); //trigger open door alarm
        doorOpenCounter = 0; //set it again to 0 so as to trigger alarm again after x time
      }
    }

    // refresh screen with delay
    unsigned long currentTime2 = millis();
    if (currentTime2 - previousTime2 > interval2) {
      previousTime2 = currentTime2;
      lcd.clear();
      lcd.setCursor(0, 0); // lcd.setCursor(column, line);
      lcd.print("Temperat.:" + String(temperature));
      lcd.setCursor(0, 1);
      lcd.print("Humid.:" + String(humidity));
    }

    //========================= Post data to ThingSpeak every x millis ==============================
    time = millis();
    //just use it one time, during arduino startup so as not to wait
    if (initialPost == true) {
      Serial.println("Initialial post...");
      time = interval + 1000000; //κάνω ένα offset από το interval για να βγεί στην συνέχει > interval, βλέπε παρακάτω
      initialPost = false;
      Serial.println("Give some time for WIFI-Connection...");
      delay(10000);
      Serial.println("Ok, continue...");
    }

    if (time - previousMillis > interval) {
      previousMillis = time;
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("Posting data...");
      Serial.println("Interval" + String(interval) + String(", Time:") + String(time) + String(",previousMillis:") + String(previousMillis) + String("   --   ") + String(time - previousMillis ));
      Serial.println("#mytemp:" + String(temperature) + "$" + "#myhumidity:" + String(humidity));//Ok, useless. Just was reading Serial with Python and grubbing the vars with # character

      //-----------------Post data to ThingSpeak
      String thingspeakVars = "&field1=" +  String(temperature) + "&field2=" + String(humidity) + "&field3=" + String(digitalRead(buttonPin));
      postToThingSpeak(TSWriteKey, thingspeakVars);

      //Resend if transmission is not completed
      if (error == 1) {
        goto start; //go to label "start"
      }
    }
  }
}







// **** ===================================================FUNCTIONS ========================================================== **** /
// **** ====================================================================================================================== **** /



//============================= Function: connectWiFi()  ============================================================================
boolean connectWiFi() {
  Serial.println("Connecting to WiFi...");
  ESP8266.println("AT+CWMODE=1");
  delay(2000);
  String cmd = "AT+CWJAP=\"";
  cmd += SSID;
  cmd += "\",\"";
  cmd += PASS;
  cmd += "\"";
  ESP8266.println(cmd);
  Serial.println(cmd);
  delay(5000);
  if (ESP8266.find("OK")) {
    Serial.println("Connected to WiFi!");
    return true;
  } else {
    Serial.println("Unable to connect to WiFi!");
    return false;
  }
}


//==========================Function: Update thingspeak ============================================================================
void postToThingSpeak(String thingspeakWriteKey, String variables) {
  Serial.println("Posting data to ThingSpeak...");
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += IP;
  cmd += "\",80";
  ESP8266.println(cmd);
  Serial.println(cmd);
  delay(2000);
  if (ESP8266.find("Error")) {
    Serial.println("Error posting data to thingspeak");
    return;
  }
  cmd = "GET /update?api_key=" + thingspeakWriteKey ;
  cmd += variables;    //variables
  cmd += "\r\n"; //terminate
  ESP8266.print("AT+CIPSEND=");
  ESP8266.println(cmd.length());
  if (ESP8266.find(">")) {
    Serial.println(cmd);
    ESP8266.print(cmd);
    delay(1000);// that's critical, else WIFI cannot connect...
    Get_reply();
  }
  else {
    Get_reply();
    ESP8266.println("AT+CIPCLOSE");
    //Resend...
    error = 1;
  }
}




//========================== Function: TemperatureAlarm============================================================================
void TemperatureAlarm(boolean SMS) {
  digitalWrite(ledBuzzer, HIGH); //turn on the buzzer
  tone(ledBuzzer, 2349);
  //tone(ledBuzzer, 2093, 500); για πιο δυνατό beep
  // tone(ledBuzzer, 100, 500); ;άλλος τόνος
  //delay(10000);
  //--------------Post data to Pushing box if door is open
  String pushingboxvars = String("&temperature=" + String(temperature) + "&humidity=" + String(humidity) + "&door=" + String(digitalRead(buttonPin))); //first constract the variables
  updatePushingBox("vFE08EDD5792D2DA", pushingboxvars );//call pushingbox post function

  //SMS
  if (sentSMS == true) {
    updatePushingBox("vCFBC1C37FBC5B2D", pushingboxvars );//call pushingbox post function (ONLY FOR SMS)
  }


}



// ===================== Function: PushingBox =========================================================================
void updatePushingBox(String key, String variables) {
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += "api.pushingbox.com";
  cmd += "\",80";
  ESP8266.println(cmd);
  Serial.println(cmd);
  delay(2000);
  if (ESP8266.find("Error")) {
    Serial.println("Error posting data to pushingbox");
    return;
  }
  
  cmd = "GET /pushingbox?devid=";  
  cmd += key;  
  cmd += variables;  
  cmd += " HTTP/1.1\r\nHost: api.pushingbox.com\r\nUser-Agent: Arduino\r\nConnection: close\r\n\r\n";
    
  //cmd = "GET /pushingbox?devid=" + String(key) + String(variables) + " HTTP/1.1\r\nHost: api.pushingbox.com\r\nUser-Agent: Arduino\r\nConnection: close\r\n\r\n";
  //cmd = "GET /pushingbox?devid=vFE08EDD5792D2DA&temperature=5.5&humidity=90.3&door=1 HTTP/1.1\r\nHost: api.pushingbox.com\r\nUser-Agent: Arduino\r\nConnection: close\r\n\r\n";
  Serial.println(cmd);
  
  ESP8266.print("AT+CIPSEND=");
  ESP8266.println(cmd.length());
  delay(1000);// that's critical, else WIFI cannot connect...

  ESP8266.print(cmd);
  //delay(1000);

  Get_reply();
  ESP8266.println("AT+CIPCLOSE");
}





//=====================Function: Get_reply ================================================================================*/
//Get response from server
String Get_reply (void)
{
  String reply = "";
  while (!ESP8266.available())
  {
    ESP8266.print('.');
    delay(200);         //could be reduced
  }

  while (ESP8266.available())
  {
    char c = ESP8266.read();
    Serial.print(c);
    reply.concat(c);
  }
  return (reply);
}





//============== function: checkTempError ================================================================================*/
//Check if there is a problem with the temperature
boolean checkTempError(float temperature) {
  boolean errorState = false;
  if (temperature > highTemp  || temperature < lowTemp)
  {
    errorState = true;
  }
  else
  {
    errorState = false;
  }
  //Serial.println( "Temperature state error:"+String(errorState));
  return errorState;
}
