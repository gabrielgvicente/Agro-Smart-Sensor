#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

//const char* ssid = "GLOBAL-VISITANTE";
//const char* password = "GS131313";
const char* ssid = "AAAAAAA";
const char* password = "11111";
const char* mqtt_server = "mqtt.tago.io";
const char* mqtt_token = "655fa59d-8713-468e-ba4a-c346e295762e";
const char* mqtt_user = "device-token";

uint8_t Broadcast[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];

int channel; 

int counter = 0;

String macString;

enum MessageType {PAIRING, DATA,};
MessageType messageType;

enum DeviceType {MESTRE, UMIDADE, TEMPERATURA, NIVEL};
DeviceType DeviceType = MESTRE;

typedef struct struct_message {
  uint8_t msgType;
  uint8_t Device_Type;
  float temp;
  float hum;
  bool on_off;
} struct_message;
struct_message DataSent;
struct_message DataRecv;

typedef struct struct_pairing {   
    uint8_t msgType;
    uint8_t Device_Type;
    uint8_t macAddr[6];
    uint8_t channel;
} struct_pairing;
struct_pairing pairingData;

bool addPeer(const uint8_t *peer_addr) 
{
  bool exists = esp_now_is_peer_exist(peer_addr);
  if (exists) 
  {
    Serial.println("Dispositivo já pareado");
    return true;
  }
  else
  {
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    peer.channel = channel;
    peer.encrypt = 0;
    memcpy(peer.peer_addr, peer_addr, 6);
    esp_err_t addStatus = esp_now_add_peer(&peer);
    if (addStatus == ESP_OK) {
      Serial.println("Pareado");
      return true;
    }
    else 
    {
      Serial.println("Falha ao parear");
      return false;
    }
  }

} 

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
{
}

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *Data, int len) 
{ 
  StaticJsonDocument<1000> root;
  char payload[100];
  uint8_t type = Data[0];       // first message byte is the type of message 
  switch (type) 
  {
    case DATA :                           // the message is data type
    {
      memcpy(&DataRecv, Data, sizeof(DataRecv));
      char *device = (char *) malloc(17 * sizeof(char)); 
      sprintf(device,"%x%x%x%x%x%x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
      char *group = (char *) malloc(17 * sizeof(char)); 
      sprintf(group,"%x:%x:%x:%x:%x:%x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
      switch(DataRecv.Device_Type)
      {
        case UMIDADE:
          root["variable"] = String(device)+"_umidade";
          root["value"] = DataRecv.hum;
          root["unit"] = "%";
          root["group"] = group;
          serializeJson(root, payload);
          client.publish("umidade", payload);
          break;
        case NIVEL:
          root["variable"] = String(device)+"_nivel";
          root["value"] = DataRecv.temp;
          root["unit"] = "CM";
          root["group"] = group;
          serializeJson(root, payload);
          client.publish("nivel", payload);
          break;
        case TEMPERATURA:
          root["variable"] = String(device)+"_temperatura";
          root["value"] = DataRecv.temp;
          root["unit"] = "°C";
          root["group"] = group;
          serializeJson(root, payload);
          client.publish("temperatura", payload);
          break;
      }

      break;
    }
    case PAIRING:                            // the message is a pairing request 
      memcpy(&pairingData, Data, sizeof(pairingData));
      if (pairingData.Device_Type != MESTRE) 
      {     // do not replay to server itself
        if (pairingData.msgType == PAIRING) 
        { 
          // Server is in AP_STA mode: , pairingData.channepeers need to send data to server soft AP MAC address
          addPeer(mac_addr);  
          WiFi.softAPmacAddress(pairingData.macAddr); 
          pairingData.channel = channel;
          pairingData.Device_Type = DeviceType;
          esp_now_send(mac_addr, (uint8_t *) &pairingData, sizeof(pairingData));
        }  
      }  
      break; 
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "IotSensorMestre";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect("ESP32", mqtt_user, mqtt_token)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      //client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(0, INPUT_PULLUP);

  // Initialize Serial Monitor
  Serial.begin(115200);

  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.softAP("CORNO GRITANDO POR FUTEBOL", "1KSWDHFIFHDKN");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  channel = WiFi.channel();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  if (esp_now_init() != ESP_OK) 
  {
    esp_restart();
    return;
  }
  Serial.println("Iniciando");
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

}

void loop() {
   // if (!client.connected()) {
    //reconnect();
  //}
  //client.loop();
}