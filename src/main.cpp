#include <Arduino.h>

#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.inrim.it", 3600, 60000);

#include <EspMQTTClient.h>
EspMQTTClient client(
  "FEDDYNVENTOR88",
  "sliceofpizza88",
  "10.0.1.2",  // MQTT Broker server ip
  "",   // Can be omitted if not needed
  "",   // Can be omitted if not needed
  "alarmEsp8266",     // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);


#define pooling true  //send values efficiently periodically
unsigned int lastSent = 0;
int sendInterval = 2000;

unsigned int clockMec = 0; //conserva millis dell'ultimo tick del secondo, vedi riga

int silenceBeforeStorm = 15000;
int stormTimeBordello = 180000;
int intrusionContinuousInterval = 60000;
unsigned long armingTime = 30000;

#define continuousTimes 3
byte continuousCount = 0;

long int inputGotten = 0;
long int outputIntrusion = 0;
unsigned long timerBeforeArming = 0;

int h = 0;
int m = 0;
int s = 0;
int dow = 0;

bool armedState = false;

void setup() {
  Serial.begin(9600);

  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overrited with enableHTTPWebUpdater("user", "password").
  //client.enableLastWillMessage("TestClient/lastwill", "I am going offline");  // You can activate the retain flag by setting the third parameter to true

  pinMode(4, OUTPUT);  //onboard
  pinMode(5, INPUT_PULLUP);
  digitalWrite(4, LOW);
  
}

void onConnectionEstablished() {
  timeClient.begin();
  timeClient.update();
  h = timeClient.getHours();
  m = timeClient.getMinutes();
  s = timeClient.getSeconds();
  dow = timeClient.getDay();

  Serial.println("NTP Gathered = "+String(h)+":"+String(m)+":"+String(s)+" #"+String(dow));
  
  client.subscribe("alarmesp/start", [](const String & payload) {
    if (!armedState && payload=="1")
      timerBeforeArming = millis();
  });
  client.subscribe("alarmesp/armed", [](const String & payload) {
    if (payload=="0"){
      armedState = false;
      digitalWrite(4, LOW);
      inputGotten = 0;
      outputIntrusion = 0;
      continuousCount = 0;
      timerBeforeArming = 0;
      client.publish("alarmesp/intrusion", "0");
    }
  });
  client.subscribe("alarmesp/set/silence", [](const String & payload) {
    silenceBeforeStorm = (payload.toInt());
  });
  client.subscribe("alarmesp/set/storm", [](const String & payload) {
    stormTimeBordello = (payload.toInt());
  });
  client.subscribe("alarmesp/set/timer", [](const String & payload) {
    armingTime = (payload.toInt());
  });

  // Subscribe to "mytopic/wildcardtest/#" and display received message to Serial
  //client.subscribe("currentesp/wildcardtest/#", [](const String & topic, const String & payload) {
  //  Serial.println("(From wildcard) topic: " + topic + ", payload: " + payload);
  //});

}

void loop()
{
  client.loop();

  if (digitalRead(5) && !armedState && timerBeforeArming!=0 && inputGotten==0 && outputIntrusion==0 ){
    timerBeforeArming=millis();
  }
  if (millis()-timerBeforeArming>armingTime && timerBeforeArming!=0){
    armedState=true;
    timerBeforeArming = 0;
    client.publish("alarmesp/armed", "1");
  }

  if (digitalRead(5) && armedState && inputGotten==0 && outputIntrusion==0 ){
    inputGotten = millis();
  }

  // if ((millis()-inputGotten > silenceBeforeStorm) && outputIntrusion==0 && inputGotten!=0){
  // if ((millis()-inputGotten > silenceBeforeStorm) && inputGotten!=0){
  if ( armedState && ((millis() > silenceBeforeStorm+inputGotten) && inputGotten!=0)){
    digitalWrite(4, HIGH);
    client.publish("alarmesp/intrusion", "1");
    outputIntrusion = millis();
    Serial.println(inputGotten);
    inputGotten = 0;
  }

  if ((millis() - outputIntrusion > stormTimeBordello) && outputIntrusion!=0 && inputGotten==0) {
    digitalWrite(4, LOW);
    client.publish("alarmesp/intrusion", "0");
    if (continuousCount<continuousTimes){
      inputGotten = millis()+intrusionContinuousInterval;  // continua
      continuousCount++;
    } else {
      inputGotten = 0;
      outputIntrusion = 0;
      continuousCount = 0;
    }
  }
  
  if (millis() - clockMec > 999) {
  
    s++;
    if (s>=60){
      m++;
      s=0;
    }
    if (m>=60){
      h++;
      m=0;
    }
    if (h>=24){
      s=0;m=0;h=0;
      dow++;
    }
    if (dow==8){
      dow=1;
    }

    if (m==0 && h==3 && s==0){
      timeClient.update();
    }

    // Serial.print(m>=turnOffMin);
    // Serial.print(h>=turnOffHour);
    // Serial.print(goPowerOff);
    // Serial.print(Irms<currentBottomThreshold);
    // Serial.print(((char)daynot[dow-1])=='1');
    // Serial.println(" Timer ongoing = "+String(h)+":"+String(m)+":"+String(s));
    
    // ...

    clockMec = millis();
  }

  if (digitalRead(5))
    client.publish("alarmesp/sensor", "1");

  // if ((millis() - lastSent > sendInterval) && pooling) {

  //   client.publish("alarmesp/sensor", digitalRead(5)?"1":"0");
  //   // comunica lo stato continuo ma non al cambiamento
    
  //   lastSent = millis();
  // }
}



String double2string(double n, int ndec) {
    String r = "";

    int v = n;
    r += v;     // whole number part
    r += '.';   // decimal point
    int i;
    for (i=0;i<ndec;i++) {
        // iterate through each decimal digit for 0..ndec 
        n -= v;
        n *= 10; 
        v = n;
        r += v;
    }

    return r;
}