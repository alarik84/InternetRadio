#include <Audio.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <clientSecrets.h>
#include <PubSubClient.h>
#include <Wifi.h>

#define I2S_DOUT 2
#define I2S_BCK 4
#define I2S_LCK 15

TaskHandle_t Task1;

Audio audio;
WiFiClient wifiClient;

PubSubClient mqttClient(wifiClient);

const char *deviceName = "InternetPlayer";
String clientID = "devices/" + String(deviceName);
String sensorsMessage = clientID + String("/sensors");
String availabilitySuffix = "/state";
String availabilityTopicStr = clientID + availabilitySuffix;
const char *availabilityTopic = availabilityTopicStr.c_str();
const char *birthMessage = "online";
const char *lwtMessage = "offline";
const char *connectedURL = "devices/InternetPlayer/connectedURL";

#include <mqttDiscoveryTopics.h>

void publishBirthMessage()
{
  Serial.println();
  Serial.print("Publishing birth message \"");
  Serial.print(birthMessage);

  Serial.print("\" to ");
  Serial.print(availabilityTopic);
  Serial.println();
  mqttClient.publish(availabilityTopic, birthMessage, true);
}

void audioPlayPause()
{
  Serial.println("Audio pause/resume...");
  audio.pauseResume();
}

void audioStop()
{
  Serial.println("Audio stop...");
  audio.stopSong();
}

void audiotSetVolume(uint8_t volValue)
{
  Serial.println("Changing volume...");
  audio.setVolume(volValue);
  // mqttClient.publish(deviceName + char("/volume"), String(volValue).c_str(), true);
}

void audioConnect(char *webURL)
{
  uint8_t initialVolume = audio.getVolume();
  Serial.println("Connecting url...");
  Serial.println(webURL);
  audioStop();
  audiotSetVolume(0);
  delay(50);
  audio.connecttohost(webURL);
  audiotSetVolume(initialVolume);
  mqttClient.publish(connectedURL, webURL, true);
  Serial.println("Connected...");
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{

  char payloadCombined[length];
  String payloadCombinedStr;

  Serial.println("MQTT command received...");

  for (int i = 1; i < length; i++)
  {
    payloadCombinedStr += (char)payload[i];
    if (i > 1 && (char)payload[i + 1] == '"')
    {
      break;
    }
  }

  for (int i = 0; i < length; i++)
  {
    payloadCombined[i] = payload[i];
  }

  Serial.println("MQTT message received: " + String(topic));
  Serial.println("MQTT payload: " + String(payloadCombined));

  if (String(topic).indexOf("WebStream") > 0 && String(payloadCombined).length() > 0)
  {
    audioConnect((char *)payloadCombinedStr.c_str());
  }
  if (String(topic).indexOf("PlayPause") > 0)
  {
    audioPlayPause();
  }
  if (String(topic).indexOf("Stop") > 0)
  {
    audioStop();
  }
  if (String(topic).indexOf("Volume") > 0)
  {
    audiotSetVolume((uint8_t)atoi(payloadCombined));
  }
  if (String(topic).indexOf("SystemRestart") > 0 && (char)payload[0] == '1')
  {
    ESP.restart();
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    Serial.println("Subscribing topics...");
    // Attempt to connect
    if (mqttClient.connect(deviceName, mqttUser, mqttPasswd, availabilityTopic, 0, true, lwtMessage))
    {
      // Once connected, publish an announcement...
      // ... and resubscribe

      mqttClient.subscribe("devices/InternetPlayer/cmd/PlayPause");
      mqttClient.subscribe("devices/InternetPlayer/cmd/Stop");
      mqttClient.subscribe("devices/InternetPlayer/cmd/SystemRestart");
      mqttClient.subscribe("devices/InternetPlayer/cmd/Volume");
      mqttClient.subscribe("devices/InternetPlayer/cmd/WebStream");
      publishBirthMessage();
    }
    else
    {
      Serial.println("MQTT connection failed, waiting 5 seconds to try again...");
      delay(5000);
    }
  }
}

void setup_wifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPassword);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
  }
  publishBirthMessage();
}

void TaskForCore0( void * parameter) {
  Serial.println("Task on core 0 runnung...");
  for(;;) {
    if (!mqttClient.connected())
    {
      Serial.print("Reconnecting...");
      reconnect();
    }
    mqttClient.loop();
    ArduinoOTA.handle();
  }
} 

void loop()
{
  audio.loop();
}

void setup()
{

  Serial.begin(115200);
  while (!Serial)
  {
    delay(100);
  }

  Serial.print("Connecting WiFi...");
  setup_wifi();

  Serial.println("Setting OTA...");
  ArduinoOTA.setPassword(otaPassword);
  ArduinoOTA.setHostname(deviceName);
  ArduinoOTA.begin();

  Serial.println("Setting up audio module...");
  audio.setPinout(I2S_BCK, I2S_LCK, I2S_DOUT);
  audio.setVolume(12); // 0...21
  //audio.connecttohost("http://ml01.cdn.eurozet.pl/mel-krk.mp3");

  Serial.println("Setting MQTT server parameters...");
  mqttClient.setServer(mqttServer, mqttPort);
  Serial.println("Setting MQTT callback procedure...");
  mqttClient.setCallback(mqttCallback);
  // mqttClient.connect(deviceName, mqttUser, mqttPasswd);
  Serial.println("Setting callback...");
  reconnect();
  
  xTaskCreatePinnedToCore(
      TaskForCore0, /* Function to implement the task */
      "Task1", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &Task1,  /* Task handle. */
      0); /* Core where the task should run */
}
