#include <FirebaseESP32.h>
#include <WiFi.h>
#include "EEPROM.h"
#include "BluetoothSerial.h"

#define WIFI_TIMEOUT        10000
#define CONNECTION_TIMEOUT  10000
#define COUNTER_THRESHOLD   20

#define FB_HOST "<Firebase host URL>"
#define FB_AUTH "<Firebase host Key>"

#define TRIG 22
#define ECHO 23
#define SLED 15

const char* BTname = "PARKSEN";
// address -> WIFI_SSID: 0, WIFI_PW: 50
String WIFI_SSID;
String WIFI_PW;
String DEVICE_ID = "NONAME";
int parked = 0;
int LEDstate = LOW;

BluetoothSerial BTSerial;
FirebaseData fbdata;

bool connectWiFi()
{
  WIFI_SSID = EEPROM.readString(0);
  WIFI_SSID.trim();
  WIFI_PW = EEPROM.readString(50);
  WIFI_PW.trim();
  DEVICE_ID = EEPROM.readString(100);
  DEVICE_ID.trim();
  Serial.printf("%s, %s, %s\n", WIFI_SSID, WIFI_PW, DEVICE_ID);
  WiFi.begin(WIFI_SSID, WIFI_PW);
  for (int i = 0; i < 10; i++)
  {
    Serial.print(".");
    delay(1000);
    if (WiFi.status() == WL_CONNECTED)
    {
      parked = 0;
      Firebase.begin(FB_HOST, FB_AUTH);
      Firebase.reconnectWiFi(true);
      return true;
    }
  }
  return false;
}

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(150);
  //EEPROM.writeString(0, "");
  //EEPROM.writeString(50, "");
  //EEPROM.commit();

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(SLED, OUTPUT);
  
  if (!connectWiFi())
  {
    Serial.println("Cannot find WiFi");
    Serial.println("Bluetooth is now available");
    BTSerial.begin(BTname);
  }
  else
  {
    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }
}

void loop()
{
  if (BTSerial.available())
  {
    String line1 = BTSerial.readStringUntil(',');
    String line2 = BTSerial.readStringUntil(',');
    String line3 = BTSerial.readStringUntil('\n');
    line1.trim();
    line2.trim();
    line3.trim();
    Serial.printf("SSID: %s\nPW: %s\nID: %s\n", line1, line2, line3);
    EEPROM.writeString(0, line1);
    EEPROM.writeString(50, line2);
    EEPROM.writeString(100, line3);
    EEPROM.commit();
    Serial.println("Attempt to connect WiFi");
    if (!connectWiFi())
    {
      Serial.println("Cannot find WiFi");
      Serial.println("Bluetooth is now available");
      BTSerial.println("null");
      //BTSerial.begin(BTname);
    }
    else
    {
      Serial.println("WiFi connected");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      BTSerial.end();
      digitalWrite(SLED, HIGH);
    }
  }
  else if (WiFi.status() == WL_CONNECTED)
  {
    digitalWrite(TRIG, HIGH);
    delay(10);
    digitalWrite(TRIG, LOW);
    double dist = pulseIn(ECHO, HIGH) * 17.0 / 1000.0;
    static int ucnt = 0;
    if (parked < 0) parked = 0;
    if ((dist < 20.0) ^ parked != 0) //parking: 1 ^ 0, unparking: 0 ^ 1
    {
      digitalWrite(SLED, LOW);
      if (++ucnt >= COUNTER_THRESHOLD)
      {
        ucnt = 0;
        parked ^= 1;
        Firebase.setInt(fbdata, String(DEVICE_ID), parked);
      }
      delay(100);
      digitalWrite(SLED, HIGH);
    }
    else
    {
      ucnt = 0;
      digitalWrite(SLED, HIGH);
    }
    Serial.printf("dist: %f\ncount: %d\nparked: %d\n", dist, ucnt, parked);
  }
  else
  {
    LEDstate ^= 1;
    digitalWrite(SLED, LEDstate);
    if (parked != -1)
    {
      parked = -1;
      BTSerial.begin(BTname);
      WiFi.begin(WIFI_SSID, WIFI_PW);
    }
  }
  delay(500);
}
