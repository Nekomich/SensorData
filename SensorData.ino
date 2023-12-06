// library for temperature and humidity
#include "DHT.h"

// library for WLAN and time
#include "WiFi.h"
#include "time.h"

// library for MQTT
#include "PubSubClient.h"


// DHT22 initialize
#define DHTTYPE DHT22
// ESP32 GPIO pin connected to Temp/Humidity Sensor's pin
#define DHTPIN 22
DHT dht(DHTPIN, DHTTYPE);

// HC-SR04 (ultrasonic sensor) initialize
// ESP32 GPIO pin connected to Ultrasonic Sensor's TRIG pin
#define Trigger_AusgangsPin 21 
// ESP32 GPIO pin connected to Ultrasonic Sensor's ECHO pin
#define Echo_EingangsPin 23

int maximumRange = 600;
int minimumRange = 2;
long Abstand;
long Dauer;


// MQTT-Server and Topics
const char* mqtt_server = "192.168.23.228";
#define RANGE_TOPIC   "J1.06.1/HWDR/Range"
#define TIME_TOPIC    "J1.06.1/HWDR/Time"
#define TEMP_TOPIC    "J1.06.1/HWDR/Temp"
#define HUMI_TOPIC    "J1.06.1/HWDR/Humi"

// Local network configuration of esp32
// should be pc +100 for IP
IPAddress localIP(192, 168, 23, 212);
IPAddress gateway(192, 168, 23, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 10, 222);

// WLAN preferences
const char* ssid = "FRITZ!Box Fon WLAN 7390";
const char* password = "3735033917067700";

// Create an instance of PubSubClient client
WiFiClient espClient;
PubSubClient client(espClient);

// Variables defined to fetch ntp time
const char* ntpServer = "192.168.10.222";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;



void setup() {
  // Configures static IP address
  if (!WiFi.config(localIP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }

  connectToNetwork();
  WiFi.setSleep(false);

  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());

  //  WiFi.disconnect(true);
  //  Serial.println(WiFi.localIP());

  // configure the MQTT server with IPaddress and port
  client.setServer(mqtt_server, 1883);

  // init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  pinMode(Trigger_AusgangsPin, OUTPUT);
  pinMode(Echo_EingangsPin, INPUT);
  Serial.begin (9600);

  // DHT22 Mearsurement will be started
  dht.begin();
}


void connectToNetwork() {
  // Configures static IP address
  if (!WiFi.config(localIP, gateway, subnet, primaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(2500);
    Serial.println("Establishing connection to WiFi..");
    Serial.println("SSID: " + String(ssid) + " Passwort: " + String(password));
    delay(5000);
  }
  Serial.println("Connected to network");
}

void mqttconnect() {
  // Loop until reconnected
  while (!client.connected()) {
    Serial.println("MQTT connecting... ");

    // Connect now
    // Convert client string into array of character for open connection
    if (client.connect(String("HWDR_ESP32").c_str())) {
      Serial.println("Connected!");
    } else {
      Serial.println("Failed, status code =" + String(client.state()) + ". Try again in 5 seconds...");
    }

    // wait 5 sec
    delay(5000);
  }
}


void loop() {
  // If client was disconnected, try to reconnect
  if (WiFi.status() != WL_CONNECTED) {
    connectToNetwork();  
  }
  if (!client.connected()) {
    mqttconnect();
  }

  // START -- console output of humidity and temperature
  Serial.println("\n---------Temperatur und Luftfeuchtigkeit (DHT22) ----------");

  // Measurement of humidity
  float h = dht.readHumidity();
  // Measurement of temperature
  float t1 = dht.readTemperature();

  // The measurements will be tested of errors
  // If an error is detected, a message will be displayed
  if (isnan(h) || isnan(t1)) {
    Serial.println("Error while reading the sensor");
    return;
  }
  
  // Output at the serial console
  Serial.println("Luftfeuchtigkeit: " + String(h) + " %\tTemperatur: " + String(t1) + "°C");
  Serial.println("-----------------------------------------------------------");
  // END -- console output of humidity and temperature

  client.publish(TEMP_TOPIC, String(t1).c_str(), true);
  client.publish(HUMI_TOPIC, String(h).c_str(), true);

  // fetch current time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  // 50 chars should be enough for formatted time value
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%d.%m.%y, %H:%M:%S", &timeinfo);

  // Destination measurement will be started with 10us trigger signals
  digitalWrite(Trigger_AusgangsPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(Trigger_AusgangsPin, LOW);

  // Waiting on echo GPIO pin until signal was returned
  // and time will be measured how long it is active
  Dauer = pulseIn(Echo_EingangsPin, HIGH);

  // Calculating distance with measured time
  Abstand = Dauer/58.2;

  // START -- console output of distance and time
  Serial.println("\n---------Abstand und Zeit (HC-SR04) -----------------------");

  // Check if calculated value is in valid range
  if (Abstand >= maximumRange || Abstand <= minimumRange) {
    // if value is out of range display error message
    Serial.println("Abstand außerhalb des Messbereichs");
    Serial.println("-----------------------------------------------------------");
  } else {
    // if value valid it is printed to std out
    Serial.println("Der Abstand betraegt: " + String(Abstand) + "cm");
    Serial.println("Die Dauer betraegt: " + String(Dauer) + "ms");
    Serial.println("-----------------------------------------------------------");

    client.publish(RANGE_TOPIC, String(Abstand).c_str(), true);
    delay(5000);
    client.publish(TIME_TOPIC, String(Dauer).c_str(), true);
  }
  // END -- console output of distance and time


  // wait 10 sec
  delay(10000);
}
