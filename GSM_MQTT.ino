#define TINY_GSM_MODEM_SIM808

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>

#define SerialMon Serial
#define TINY_GSM_DEBUG SerialMon
#define LED_PIN 13

// Tum AT Commandlari gormek icin tanimalama gerceklestirilmeli
// #define DUMP_AT_COMMANDS

uint32_t lastReconnectAttempt = 0;
int ledStatus = LOW;

SoftwareSerial SerialAT(2, 3);  // RX, TX

//AutoBaud min max degerleri
//Dogru AutoBaud degeri bulunduktan sonra
//Production asamasinda modem.SetBaud() kullanilmalidir
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200

// Add a reception delay, if needed.
// This may be needed for a fast processor at a slow baud rate.
// #define TINY_GSM_YIELD() { delay(2); }

#define TINY_GSM_USE_GPRS true

//Sim kart icin pin numarasi
#define GSM_PIN "1840"

//GPRS Baglanti Ayarlari
const char apn[]      = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

// MQTT Broker Adres Ayari
const char* broker = "broker.mqttdashboard.com";

//MQTT Brokerinda pub ve sub topicleri
const char* topicLed       = "GsmClientTest/led";
const char* topicInit      = "GsmClientTest/init";
const char* topicLedStatus = "GsmClientTest/ledStatus";

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

TinyGsmClient client(modem);
PubSubClient  mqtt(client);

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  SerialMon.print("Gelen Mesaj [");
  SerialMon.print(topic);
  SerialMon.print("]: ");
  SerialMon.write(payload, len);
  SerialMon.println();

  //Gelen mesajin topic kontrolu
  if (String(topic) == topicLed) {
    ledStatus = !ledStatus;
    digitalWrite(LED_PIN, ledStatus);
    mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
  }
}

boolean mqttConnect() {
  SerialMon.print(broker);
  SerialMon.print(" Adresine Baglanti Kuruluyor");

  // Connect to MQTT Broker
  boolean status = mqtt.connect("GsmClientTest");

  // Authentication icin bu fonksiyon kullanilmalidir
  // boolean status = mqtt.connect("GsmClientName", "mqtt_user", "mqtt_pass");

  if (status == false) {
    SerialMon.println(" hata olustu");
    return false;
  }
  SerialMon.println(" islem basarili");
  mqtt.publish(topicInit, "GsmClientTest baslatildi");
  mqtt.subscribe(topicLed);
  return mqtt.connected();
}


void setup() {
  SerialMon.begin(115200);
  delay(10);

  pinMode(LED_PIN, OUTPUT);

  // !!!!!!!!!!!
  // Set your reset, enable, power pins here
  // !!!!!!!!!!!

  SerialMon.println("Bekleniyor...");

  // Set GSM module baud rate
  TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  // SerialAT.begin(9600);
  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Modem baslatiliyor...");
  modem.restart();
  // modem.init();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Bilgisi: ");
  SerialMon.println(modemInfo);

#if TINY_GSM_USE_GPRS
  //PIN numarasi aktif ise unlock islemi icin gerekli ifdef
  if (GSM_PIN && modem.getSimStatus() != 3) { modem.simUnlock(GSM_PIN); }
#endif

  SerialMon.print("Ag baglantisi bekleniyor...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" hata olustu");
    delay(10000);
    return;
  }
  SerialMon.println(" basarili");

#if TINY_GSM_USE_GPRS
  SerialMon.print(F("GPRS Baglantisi Kuruluyor... "));
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" hata olustu");
    delay(10000);
    return;
  }
  SerialMon.println(" basarili");

  if (modem.isGprsConnected()) { SerialMon.println("GPRS Baglantisi Saglandi"); }
#endif

  //MQTT Broker Detaylari
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);"
}

void loop() {
    //GPRS Baglanti Kontrolu
    if (!modem.isGprsConnected()) {
      SerialMon.println("GPRS disconnected!");
      SerialMon.print(F("Connecting to "));
      SerialMon.print(apn);
      if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
        delay(10000);
        return;
      }
}

  if (!mqtt.connected()) {
    SerialMon.println("=== MQTT Baglantisi Kurulamadi ===");
    // Reconnect every 10 seconds
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) { lastReconnectAttempt = 0; }
    }
    delay(100);
    return;
  }
  mqtt.loop();
}
