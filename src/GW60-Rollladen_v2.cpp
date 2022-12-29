#pragma GCC diagnostic warning "-Wwrite-strings"
#pragma GCC diagnostic warning "-Wparentheses"
//#pragma GCC diagnostic ignored "-Wsign-compare"

// #include <Arduino.h>
#include <PubSubClient.h> 
#include "FS.h"
#include <EEPROM.h>
#include <ESP8266HTTPUpdateServer.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>

// -- Definiere, ob wir uns im Test-Modus befinden (z.B. um neue Funktionen zu überprüfen)
bool TestMode = false;
const char* TestModeVerbal = "deaktiviert";

// -- Initialer Name des Gerätes. Wird auch als WLAN-SSID im AP-Modus benutzt SSID of the own Access Point.
const char thingName[] = "GW60-ESP";

// -- Initiales Passwort zum Verbinden mit dem Gerät, wenn es im AP-Modus arbeitet.
const char wifiInitialApPassword[] = "GW60-ESP";

#define STRING_LEN 128
#define NUMBER_LEN 32

// -- Konfigurations-Version. Dieser Wert sollte geändert werden, wenn sich etwas an der Konfigurations-Struktur geändert hat.
#define CONFIG_VERSION "GW_v5"

// -- Status-Indicator Pin.
//    Erst komplett ein, Während des WLAN-Aufbaus blinkt es schnell,
//    wenn mit WLAN verbunden, einmal Blinken alle 8 Sekunden.
#define STATUS_PIN D7

// -- Wenn im Bootvorgang auf LOW, startet das Gerät im AP-Modus mit dem initialen Passwort
//    z.B. wenn das Passwort vergessen wurde
#define CONFIG_PIN D0

// -- MQTT Topic Prefix
#define MQTT_TOPIC_PREFIX mqttTopicValue

// -- Callback Deklarationen.
void up();
void down();
void savemaxcount();
void savezeropos();
void currentdirection();
void statedirection();
void currentposition();
void countposition();
void newposition();
void callback(char const* topic, byte* payload, unsigned int length);
void reconnect();
void mqttSend(char const* mqtt_topic,char const* mqtt_content,bool mqttRetain);
void handleRoot();
void wifiConnected();
void configSaved();

boolean formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);

DNSServer dnsServer;
WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient wifiClient;

char mqttServerValue[STRING_LEN];
//char mqttPortValue[NUMBER_LEN];
char mqttUserValue[STRING_LEN];
char mqttPasswordValue[STRING_LEN];

char MaxPosMQTTValue[STRING_LEN];
String SaveToMQTT = "";

char mqttTopicValue[STRING_LEN];
char mqttActionTopic[STRING_LEN];
char mqttDirectionTopic[STRING_LEN];
char mqttPercentTopic[STRING_LEN];
char mqttPositionTopic[STRING_LEN];
char mqttMaxPosTopic[STRING_LEN];

/*
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfSeparator separator2 = IotWebConfSeparator();
IotWebConfParameter mqttServer = IotWebConfParameter("MQTT Server", "mqttServer", mqttServerValue, STRING_LEN);
//IotWebConfParameter mqttPort = IotWebConfParameter("MQTT Port", "mqttPort", mqttPortValue, NUMBER_LEN, "number", "z.B. 1883", "1883");
IotWebConfParameter mqttUser = IotWebConfParameter("MQTT User", "mqttUser", mqttUserValue, STRING_LEN);
IotWebConfParameter mqttPassword = IotWebConfParameter("MQTT Passwort", "mqttPassword", mqttPasswordValue, STRING_LEN, "password");
IotWebConfParameter mqttTopic = IotWebConfParameter("MQTT Topic-Prefix", "mqttTopic", mqttTopicValue, STRING_LEN, "text", "z.B. GW60/Testraum");
*/

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

// -- Javascript block will be added to the header.
const char CUSTOMHTML_SCRIPT_INNER[] PROGMEM = "\n\
document.addEventListener('DOMContentLoaded', function(event) {\n\
  let elements = document.querySelectorAll('input[type=\"password\"]');\n\
  for (let p of elements) {\n\
  	let btn = document.createElement('INPUT'); btn.type = 'button'; btn.value = '🔓'; btn.style.width = 'auto'; p.style.width = '83%'; p.parentNode.insertBefore(btn,p.nextSibling);\n\
    btn.onclick = function() { if (p.type === 'password') { p.type = 'text'; btn.value = '🔒'; } else { p.type = 'password'; btn.value = '🔓'; } }\n\
  };\n\
});\n";
// -- HTML element will be added inside the body element.
const char CUSTOMHTML_BODY_INNER[] PROGMEM = "<div><img src='data:image/jpeg;base64,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'/></div>\n";

// -- This is an OOP technique to override behaviour of the existing
// IotWebConfHtmlFormatProvider. Here two method are overridden from
// the original class. See IotWebConf.h for all potentially overridable
// methods of IotWebConfHtmlFormatProvider .
class CustomHtmlFormatProvider : public iotwebconf::HtmlFormatProvider
{
protected:
  String getScriptInner() override
  {
    return
      HtmlFormatProvider::getScriptInner() +
      String(FPSTR(CUSTOMHTML_SCRIPT_INNER));
  }
  String getBodyInner() override
  {
    return
      String(FPSTR(CUSTOMHTML_BODY_INNER)) +
      HtmlFormatProvider::getBodyInner();
  }
};
// -- An instance must be created from the class defined above.
CustomHtmlFormatProvider customHtmlFormatProvider;

// IotWebConfParameterGroup group1 = IotWebConfParameterGroup("group1", "");
IotWebConfParameterGroup group2 = IotWebConfParameterGroup("group2", "MQTT Parameter");
IotWebConfTextParameter mqttServer = IotWebConfTextParameter("MQTT Server", "mqttServer", mqttServerValue, STRING_LEN);
//IotWebConfNumberParameter mqttPort = IotWebConfNumberParameter("MQTT Port", "mqttPort", mqttPortValue, NUMBER_LEN,"1883");
IotWebConfTextParameter mqttUser = IotWebConfTextParameter("MQTT User", "mqttUser", mqttUserValue, STRING_LEN);
IotWebConfTextParameter mqttPassword = IotWebConfTextParameter("MQTT Password", "mqttPassword", mqttPasswordValue, STRING_LEN);
IotWebConfTextParameter mqttTopic = IotWebConfTextParameter("MQTT Topic-Prefix", "mqttTopic", mqttTopicValue, STRING_LEN, "z.B. GW60/Testraum");
IotWebConfParameterGroup group1 = IotWebConfParameterGroup("group1", "Save Options (EEPROM or MQTT)");
IotWebConfCheckboxParameter MaxPosMQTT = IotWebConfCheckboxParameter("Save via MQTT", "checkParam", MaxPosMQTTValue, STRING_LEN,  true);

bool needMqttConnect = false;

// -- MQTT
char const* mqtt_server = mqttServerValue;
// int mqtt_port; // = atoi(mqttPortValue);
char const* mqtt_username = mqttUserValue;
char const* mqtt_password = mqttPasswordValue;
char const* mqtt_clientID = iotWebConf.getThingName();
String  MaxPos_MQTT = String(MaxPosMQTTValue);
char message_buff[100];
bool mqttRetain = false;
int mqttQoS = 1;
int mqtt_port = 1883;

// -- Variablen
int dir = 0, shadeposition = 0, count = 0, maxcount, newpercentage = 0, counted = 0, pinD6 = 0, address = 0, drivemincount = 0, drivemaxcount = 0;
char ccount[4]; char cpercentage[5];
float percentage = 0;
bool remote = false, needReset = false;;
byte value;

boolean isNumeric(String str) {  // -- Prüfen, ob der empfangene Wert nummerisch ist
    unsigned int stringLength = str.length();
 
    if (stringLength == 0) {
        return false;
    }
 
    boolean seenDecimal = false;
 
    for(unsigned int i = 0; i < stringLength; ++i) {
        if (isDigit(str.charAt(i))) {
            continue;
        }
 
        if (str.charAt(i) == '.') {
            if (seenDecimal) {
                return false;
            }
            seenDecimal = true;
            continue;
        }
        return false;
    }
    return true;
}

PubSubClient client(mqtt_server, mqtt_port, callback, wifiClient); // -- MQTT initialisieren

void up() // -- Funktion für hoch
{
  if (dir != -1)
  {
    if (TestMode) {
      Serial.println("Rollladen fährt hoch");
    }
      digitalWrite(D2, LOW);
      delay(300);
      digitalWrite(D2, HIGH);
      delay(500);
  }   
}

void down() // -- Funktion für runter
{ 
  if (dir != 1)
  {
    if (TestMode) {
      Serial.println("Rollladen fährt runter");
    }
      digitalWrite(D1, LOW);
      delay(300);
      digitalWrite(D1, HIGH);
      delay(500);
  }
}

void savemaxcount() // -- Speicherung der unteren Position
{
  if (MaxPos_MQTT  = "selected") {
    char cmaxcount[4];
    itoa( count, cmaxcount, 10 );
    mqttSend(mqttMaxPosTopic, cmaxcount, true);
    maxcount = count;
    if (TestMode) {
      Serial.print("Neue untere Position:");
      Serial.println(String(maxcount));
      Serial.println("auf MQTT-Server gespeichert");
    }
  } else {
    EEPROM.begin(4096);
    EEPROM.write (4000, count);
    EEPROM.commit();
    maxcount = EEPROM.read (4000);
    EEPROM.end();
    if (TestMode) {
      Serial.print("Neue untere Position:");
      Serial.println(String(maxcount));
      Serial.println("in EEPROM gespeichert");
    }
  }
  drivemaxcount = 0;
  digitalWrite(D2, LOW);
  delay(300);
  digitalWrite(D2, HIGH);
  delay(1000);
  digitalWrite(D2, LOW);
  delay(300);
  digitalWrite(D2, HIGH);
  delay(1000);
  digitalWrite(D1, LOW);
  delay(300);
  digitalWrite(D1, HIGH);
  delay(1000);
  digitalWrite(D1, LOW);
  delay(300);
  digitalWrite(D1, HIGH);
  delay(500);
}

void savezeropos() // -- Speicherung der oberen Position
{
  count = 0;
  if (TestMode) {
    Serial.print("Neue obere Position:");
    Serial.println(String(count));
  }
  drivemincount = 0;
  digitalWrite(D1, LOW);
  delay(300);
  digitalWrite(D1, HIGH);
  delay(1000);
  digitalWrite(D1, LOW);
  delay(300);
  digitalWrite(D1, HIGH);
  delay(1000);
  digitalWrite(D2, LOW);
  delay(300);
  digitalWrite(D2, HIGH);
  delay(1000);
  digitalWrite(D2, LOW);
  delay(300);
  digitalWrite(D2, HIGH);
  delay(500);
}

void currentdirection() // -- MQTT-Übertragung der aktuellen (geänderten) Richtung
{
  if (digitalRead(D4) == LOW && dir != 1) {
    mqttSend(mqttDirectionTopic, "Schliessen", mqttRetain);
    if (TestMode) {
      Serial.println("Neuer Status: Schliessen");
    }
    dir = 1;
  } else if (digitalRead(D3) == LOW && dir != -1) {
    mqttSend(mqttDirectionTopic, "Öffnen", mqttRetain);
    if (TestMode) {
      Serial.println("Neuer Status: Öffnen");
    }
    dir = -1;
  } else if (digitalRead(D4) == HIGH && digitalRead(D3) == HIGH && dir != 0) {
    mqttSend(mqttDirectionTopic, "Inaktiv", mqttRetain);
    if (TestMode) {
      Serial.println("Neuer Status: Inaktiv");
    }
    dir = 0;
    delay(1000);
  }
}

void statedirection() // -- Status-Antwort der Richtung
{
  if (digitalRead(D4) == LOW && dir == 1) {
    mqttSend(mqttDirectionTopic, "Schliessen", mqttRetain);
    if (TestMode) {
      Serial.println("Neuer Status: Schliessen");
    }
  } else if (digitalRead(D3) == LOW && dir == -1) {
    mqttSend(mqttDirectionTopic, "Öffnen", mqttRetain);
    if (TestMode) {
      Serial.println("Neuer Status: Öffnen");
    }
  } else if (digitalRead(D4) == HIGH && digitalRead(D3) == HIGH && dir == 0) {
    mqttSend(mqttDirectionTopic, "Inaktiv", mqttRetain);
    if (TestMode) {
      Serial.println("Neuer Status: Inaktiv");
    }
  }
}

void currentposition() // -- Aktuelle Position
{
  //char ccount[4];
  dtostrf(count, 3, 0, ccount);
  // itoa( count, ccount, 10 );
  mqttSend(mqttPositionTopic, ccount, mqttRetain);
  if (TestMode) {
    Serial.println("Aktuelle Position: " + String(ccount));
  }
  // -- Prozentwerte berechen
  percentage = round(count / (maxcount * 0.01)); 
  // char cpercentage[5];
  dtostrf(percentage, 3, 0, cpercentage);
  // itoa( percentage, cpercentage, 10 );
  mqttSend(mqttPercentTopic, cpercentage, mqttRetain);
  //percentageRounded = cpercentage;
  if (TestMode) {
    Serial.println("Aktuelle Position in Prozent: " + String(cpercentage));
  }
  
  shadeposition = count; // -- Speichern der aktuellen Position
}

void countposition() // -- Zählen der Positions-Impulse
{
  if (digitalRead(D5) == LOW && digitalRead(D3) == HIGH && dir == 1 && counted == 0) {
    count++;
    if (TestMode) {
      Serial.println("Neuer Zähler: " + String(count));
    }
    counted = 1; // -- sicherstellen, dass nur einmal pro LOW-Impuls gezählt wird
  } else if (digitalRead(D5) == LOW && digitalRead(D4) == HIGH && dir == -1 && counted == 0) {
    if (count > 0)
    {
      count--;
    }
    if (TestMode) {
      Serial.println("Neuer Zähler: " + String(count));
    }
    counted = 1; // -- sicherstellen, dass nur einmal pro LOW-Impuls gezählt wird
  } else if (digitalRead(D5) == HIGH && counted == 1) {
    counted = 0;
  }
}

void newposition() // -- Prüfen, ob ein neuer Prozent-Wert per MQTT eingegangen ist
{
  // -- wenn neuer Wert größer als aktueller ist und Rollladen aktuell gestoppt, dann starten
  if ( ( newpercentage > String(percentage).toInt() )  && dir == 0 && remote == true )
  {
    down(); // -- start
    if (TestMode) {
      Serial.println("Neuer Prozentwert unterscheidet sich, fahre runter");
    }
  // -- wenn aktueller Wert größer als neuer ist und momentan nach unten gefahren wird, dann stopp
  } else if ( ( String(percentage).toInt() > newpercentage )  && dir == 1 && remote == true )
  {
    up(); // -- stoppe jetzt
    if (TestMode) {
      Serial.println("Neuer Prozentwert ist gleich, stoppe jetzt");
    }
    newpercentage = percentage;
    remote = false;
    if (TestMode) {
      Serial.println(remote);
    }
  // -- wenn neuer Wert kleiner als aktueller ist und Rollladen aktuell gestoppt, dann starten
  } else if ( ( newpercentage < String(percentage).toInt() ) && dir == 0 && remote == true )
  {
    up(); // -- start
    if (TestMode) {
      Serial.println("Neuer Prozentwert unterscheidet sich, fahre hoch");
    }
  // -- wenn aktueller Wert kleiner als neuer ist und momentan nach oben gefahren wird, dann stopp
  } else if ( ( String(percentage).toInt() < newpercentage )  && dir == -1 && remote == true )
  {
    down(); // -- stope jetzt
    if (TestMode) {
      Serial.println("Neuer Prozentwert ist gleich, stoppe jetzt");
    }
    newpercentage = percentage;
    remote = false;
    if (TestMode) {
      Serial.println(remote);
    }
  }
}

void callback(char const* topic, byte* payload, unsigned int length) {  // -- MQTT Callback Funktion
  if (TestMode) {
    Serial.print("Neue Nachricht empfangen [");
    Serial.print(topic);
    Serial.print("] ");
  }
  int i = 0; 
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  String message = String(message_buff); // -- in String umwandeln
  if (TestMode) {
    Serial.print(message);
    Serial.println();
  }
  if (strcmp(topic,mqttMaxPosTopic)==0) {
    if (isNumeric(message)) {
      maxcount = message.toInt();
      if (TestMode) {
        Serial.print("Neue untere Position empfangen: ");
        Serial.println(String(maxcount));   
      }
    } else {
      if (TestMode) {
        Serial.print("Empfangene untere Position ist KEINE Zahl!");
      }
    }
  }
  if (strcmp(topic,mqttActionTopic)==0) {
    if (message.equals("up")) {
      up();
      mqttSend(mqttActionTopic, "Inaktiv", mqttRetain);
    
    } else if (message.equals("down")) {
      down();
      mqttSend(mqttActionTopic, "Inaktiv", mqttRetain);
    
    } else if (message.equals("stop")) {
      if (TestMode) {
        Serial.println("Stoppe Rollladen");
      }
      if (dir == -1) {
        down();    
      } else if (dir == 1) {
        up();
      }
    } else if (message.equals("state")) {
      currentposition();
      statedirection();
      if (TestMode) {
        Serial.println("Untere Position: " + String(maxcount));
      }
    } else if (message.equals("save")) {
      savemaxcount();
  
    } else if (message.equals("zeropos")) {
      savezeropos();
  
    } else if (isNumeric(message)) {
      int tempnewpercentage = message.toInt();
      if (tempnewpercentage == 99)  // -- Die niedrigste Position anfahren und abspeichern
      {
        down();
        if (TestMode) {
          Serial.println("Fahre max-position an...");
        }
        drivemaxcount = 1;
      }
      else if (tempnewpercentage == 1)  // -- Die höchste Position anfahren und abspeichern
      {
        up();
        if (TestMode) {
          Serial.println("Fahre min-position an...");
        }
        drivemincount = 1;
      }
      // -- Toleranz-Wert integrieren, um Ansteuerungen durch Rundungsfehler (Prozent) zu vermeiden
      else if ( ( ( tempnewpercentage > String(percentage).toInt() + 5 ) || ( tempnewpercentage < String(percentage).toInt() - 5 ) ) && (tempnewpercentage != 99) && (tempnewpercentage != 1) )
      {
        newpercentage = tempnewpercentage;
        remote = true;
        if (TestMode) {
          Serial.print("Neuen Prozentwert empfangen: ");
          Serial.println(String(newpercentage));   
        }
      }
    }
  }
}

// PubSubClient client(mqttServerValue, mqtt_port, callback, wifiClient); // -- MQTT initialisieren

void reconnect() {  // -- MQTT Reconnect
  // -- Loop bis wir verbunden sind
  while (!client.connected()) {
    if (TestMode) {
      Serial.print("Versuche MQTT-Verbindung aufzubauen... ");
    }
    // -- Versuche Neu-Verbindung
    if (client.connect(mqtt_clientID, mqtt_username, mqtt_password)) {
      if (TestMode) {
        Serial.print("MQTT-Server: ");
        Serial.println(mqttServerValue);
        Serial.print("MQTT-Port: ");
        Serial.println(mqtt_port);
        Serial.print("MQTT-PortValue: ");
    //  Serial.println(mqttPortValue);
    //  Serial.println("");
        Serial.println("verbunden");
      }
      // -- Re-Abonnierung
      client.subscribe(mqttMaxPosTopic, mqttQoS);
      if (TestMode) {
        Serial.printf("Abonniert: %s, QoS: %i\n", mqttMaxPosTopic, mqttQoS);
      }
      client.subscribe(mqttActionTopic, mqttQoS);
      if (TestMode) {
        Serial.printf("Abonniert: %s, QoS: %i\n", mqttActionTopic, mqttQoS);
      }
      /* -- Lade untere Position (maxcount) aus dem EEPROM
      EEPROM.begin(4096);
      maxcount = EEPROM.read (4000);
      EEPROM.end();
      Serial.print("Geladene Konfig 'maxcount'= '");
      Serial.print(String(maxcount));
      Serial.println("'");
      Serial.println(""); */
    } else {
      if (TestMode) {
        Serial.print("fehlgeschlagen, rc=");
        Serial.print(client.state());
        Serial.println(" versuche es in 5 Sekunden erneut");
        Serial.print("MQTT-Server: ");
        Serial.println(mqtt_server);
        Serial.print("MQTT-Port: ");
        Serial.println(mqtt_port);
        Serial.print("MQTT-PortValue: ");
      //  Serial.println(mqttPortValue);
      //  Serial.println("");
      }
      // -- Warte 5 Sekunden vor erneutem Versuch
      delay(5000);
    }
    if (TestMode) {
      Serial.println("");
    }
  }
}


void mqttSend(char const* mqtt_topic,char const* mqtt_content,bool mqttRetain) {  // MQTT Sende-Routine
  if (client.publish(mqtt_topic, mqtt_content, mqttRetain)) {
    if (TestMode) {
      Serial.println("Nachricht gesendet!");
    }
  }
  else {
    if (TestMode) {
      Serial.println("Senden fehlgeschlagen. Versuche neue Verbindung ...");
    }
      if (client.connected()) {
        client.publish(mqtt_topic, mqtt_content, mqttRetain);
        if (TestMode) {
          Serial.println("Nachricht gesendet!");
        }
      }
      else {
        reconnect();
      }
    if (TestMode) {
      Serial.println("");
    }
  }
}

// -- Handle Anfragen zum "/"-Pfad.
void handleRoot()
{
  // -- Lass IotWebConf das Captive Portal testen und verwalten.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive Portal Anfragen werden bedient.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"de\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>GW60-Superrollo</title></head><body>";
  s += FPSTR(CUSTOMHTML_BODY_INNER);
  s += "<ul>";
  s += "<li>MQTT Server: ";
  s += mqttServerValue;
  s += "<li>MQTT Port: ";
  s += mqtt_port;
  s += "<li>MQTT Topic-Prefix: ";
  s += mqttTopicValue;
  s += "<li>MQTT Action Topic: ";
  s += mqttActionTopic;
  s += "<li>MQTT Direction Topic: ";
  s += mqttDirectionTopic;
  s += "<li>MQTT Position-Percent Topic: ";
  s += mqttPercentTopic;
  s += "<li>MQTT Position Topic: ";
  s += mqttPositionTopic;
  s += "<li>MQTT MaxPosition Topic: ";
  s += mqttMaxPosTopic;
  s += "</ul>";
  s += "<ul>";
  s += "<li>Endpositionen per MQTT (statt im EEPROM) speichern: ";
  s += SaveToMQTT;
  s += "<li>Aktuelle Position: ";
  s += count;
  s += " (";
  s += cpercentage;
  s += "%)";
  s += "<li>Untere Position: ";
  s += maxcount;
  s += " (100%)";
  s += "<li>Test-Modus: ";
  s += TestModeVerbal;
  s += "</ul>";
  s += "Gehe zur <a href='config'>Konfigurations-Seite</a> f&uuml;r &Auml;nderungen.";
  s += "</body></html>\n";

  server.send(200, "text/html; charset=UTF-8", s);
}

void wifiConnected()
{
  needMqttConnect = true;
}

void configSaved()
{
  if (TestMode) {
    Serial.println("Konfiguration wurde aktuallisiert.");
  }
  needReset = true;
}

boolean formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
  if (TestMode) {
    Serial.println("Eingaben werden überprüft.");
  }
  boolean valid = true;
  int s = server.arg(mqttServer.getId()).length();
  if (s < 3)
  {
    mqttServer.errorMessage = "Gib bitte einen g&uuml;ltigen Server ein.";
    valid = false;
  }
  /*int p = server.arg(mqttPort.getId()).length();
  if (p < 3)
  {
    mqttPort.errorMessage = "Gib bitte einen g&uuml;ltigen Port ein.";
    valid = false;
  }*/
  int t = server.arg(mqttTopic.getId()).length();
  if (t < 3)
  {
    mqttTopic.errorMessage = "Gib bitte ein g&uuml;ltiges Topic ein.";
    valid = false;
  }
  return valid;
}

void setup() {
  if (digitalRead(D6) == LOW) {
    TestMode = true;
    TestModeVerbal = "aktiviert";
  } else {
    TestMode = false;
    TestModeVerbal = "deaktiviert";
  }
  Serial.begin(115200);
  if (TestMode) {
    TestMode = true;
    Serial.println("Start");
    Serial.println("");
  }
  group2.addItem(&mqttServer);
  //group2.addItem(&mqttPort);
  group2.addItem(&mqttUser);
  group2.addItem(&mqttPassword);
  group2.addItem(&mqttTopic);

  group1.addItem(&MaxPosMQTT);

  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  CustomHtmlFormatProvider customHtmlFormatProvider;
  iotWebConf.addParameterGroup(&group1);
  iotWebConf.addParameterGroup(&group2);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setupUpdateServer(
    [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
    [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });
  iotWebConf.getApTimeoutParameter()->visible = true;
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  //mqtt_port = atoi(mqttPort);


  /*
  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.addParameter(&separator2);
  iotWebConf.addParameter(&mqttServer);
  //iotWebConf.addParameter(&mqttPort);
  iotWebConf.addParameter(&mqttUser);
  iotWebConf.addParameter(&mqttPassword);
  iotWebConf.addParameter(&mqttTopic);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setupUpdateServer(&httpUpdater);
  iotWebConf.getApTimeoutParameter()->visible = true;
  */


  // -- IotWebConf-Konfiguration initialisieren.
  iotWebConf.init();
  //mqtt_port = atoi(mqttPortValue);
  if (TestMode) {
    Serial.print("MQTT-Server: ");
    Serial.println(mqtt_server);
    Serial.print("MQTT-Port: ");
    Serial.println(mqtt_port);
//  Serial.print("MQTT-PortValue: ");
//  Serial.println(mqttPortValue);
  }
  // -- URL-Handler auf dem Webserver aktivieren
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

  WiFi.hostname(iotWebConf.getThingName());

  // client.setServer(mqtt_server, mqtt_port);

  // -- Setze MQTT Callback-Funktion
  client.setCallback(callback);
  
  // -- Definiere Pins
  pinMode(D5, INPUT);         // Impulszähler
  pinMode(D3, INPUT_PULLUP);  // Motor fährt hoch
  pinMode(D4, INPUT_PULLUP);  // Motor fährt runter
  pinMode(D1, OUTPUT);        // Taste für runter
  digitalWrite(D1, HIGH);     // Setze Taste auf HIGH
  pinMode(D2, OUTPUT);        // Taste für hoch
  digitalWrite(D2, HIGH);     // Setze Taste auf HIGH
  pinMode(D6, INPUT_PULLUP);  // Test-Taste für Debug-Tests

  // -- Fahre zur Initialisierung den Rollladen in Position 0
  if (TestMode) {
    Serial.println("Initialisierung - Öffne Rollladen komplett...");
  }
  digitalWrite(D2, LOW);
  delay(500);
  digitalWrite(D2, HIGH); 
  count = 0; 

  // -- Bereite dynamische MQTT-Topics vor
  String temp = String(MQTT_TOPIC_PREFIX);
  temp += "/Action";
  temp.toCharArray(mqttActionTopic, STRING_LEN);
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/Direction";
  temp.toCharArray(mqttDirectionTopic, STRING_LEN);  
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/Position-Percent";
  temp.toCharArray(mqttPercentTopic, STRING_LEN);  
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/Position";
  temp.toCharArray(mqttPositionTopic, STRING_LEN); 
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/MaxPos";
  temp.toCharArray(mqttMaxPosTopic, STRING_LEN);
  
  //mqtt_port = atoi(mqttPortValue);
  //Serial.print("Geladenen Wert mqttPortValue: ");
  //Serial.println(String(mqttPortValue));
  //Serial.print("Geladenen Wert mqtt_port: ");
  //Serial.println(String(mqtt_port));

  // -- Lade untere Position (maxcount) aus dem EEPROM
  if (MaxPos_MQTT  = "selected") {
    SaveToMQTT = "aktiviert";
    if (TestMode) {
      Serial.println("Konfig 'maxcount' wird später per MQTT geladen.");
    }
  } else {
    SaveToMQTT = "deaktiviert";
    EEPROM.begin(4096);
    maxcount = EEPROM.read (4000);
    EEPROM.end();
    if (TestMode) {
      Serial.print("Geladene Konfig 'maxcount'= '");
      Serial.print(String(maxcount));
      Serial.println("'");
      Serial.println("");
    }
  }
  // -- So, alles erledigt... kann los gehen
  if (TestMode) {
    Serial.println("Initialisierung abgeschlossen.");
  }
}

void loop() {
  // -- doLoop should be called as frequently as possible.
  iotWebConf.doLoop();
  if (needReset) // -- wenn gerade eine Konfigurations-Änderung stattgefunden hat -> ESP neu starten
  {
    if (TestMode) {
      Serial.println("Reboot nach einer Sekunde.");
    }
    iotWebConf.delay(1000);
    ESP.restart();
  }
  
  // check if MQTT is connected, if not: connect
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnect();
      //client.subscribe(mqttActionTopic, mqttQoS);
      //Serial.printf("Abonniert: %s, QoS: %i\n", mqttActionTopic, mqttQoS);
    }
  }
  client.loop();

if (digitalRead(D6) == LOW) {
  TestMode = true;
  TestModeVerbal = "aktiviert";
} else {
  TestMode = false;
  TestModeVerbal = "deaktiviert";
}

/*  if (digitalRead(D6) == LOW && pinD6 == 0 && TestMode == true) {
    if (!client.connected()) {
      reconnect();
    }
    EEPROM.begin(4096);
    value = EEPROM.read(address);
    EEPROM.end();
    Serial.print(address);
    Serial.print("\t");
    Serial.print(value, DEC);
    Serial.println();
    // -- springe zur nächsten Adresse im EEPROM
    address = address + 1;
    // -- da sind nur 4096 bytes im EEPROM, von 0 bis 4095, somit
    //    setzen wir bei Adresse 4095, alles zurück auf 0
    if (address == 4095) {
      address = 0;
      pinD6 = 1;
    }
    //delay(200);
    pinD6 = 1;
  } else if (digitalRead(D6) == HIGH && pinD6 == 1) {
    pinD6 = 0;
  }  */
  
  countposition(); // -- Zähle aktuelle Position
  
  currentdirection(); // -- Aktuelle Richtung

  if ( (shadeposition != count)  && dir != 0 ){ // -- Aktuelle Position
    currentposition();
  }
    
  newposition(); //-- Neue Position

  if ( (digitalRead(D4) == HIGH)  && drivemaxcount != 0 ){ 
    savemaxcount(); // -- Neue untere Position "lernen"
  }

  if ( (digitalRead(D3) == HIGH)  && drivemincount != 0 ){ 
    savezeropos(); // -- Neue obere Position "lernen"
  }
}
