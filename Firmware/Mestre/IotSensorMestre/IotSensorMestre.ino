#include <esp_now.h>
#include <WiFi.h>
//#include "ESPAsyncWebServer.h"
//#include "AsyncTCP.h"
#include <ArduinoJson.h>

const char* ssid = "iPhone de Gabriel";
const char* password = "12345678";

int channel; 

int counter = 0;

enum MessageType {PAIRING, DATA,};
MessageType messageType;

enum DeviceType {MESTRE, UMIDADE, TEMPERATURA, NIVEL};
DeviceType DeviceType = MESTRE;

typedef struct struct_message {
  uint8_t msgType;
  uint8_t Device_Type;
  uint8_t Device_Id;
  float temp;
  float hum;
  bool on_off;
} struct_message;
struct_message DataSent;
struct_message DataRecv;

typedef struct struct_pairing {   
    uint8_t msgType;
    uint8_t Device_Type;
    uint8_t Device_Id;
    uint8_t macAddr[6];
    uint8_t channel;
} struct_pairing;
struct_pairing pairingData;

typedef struct {
  uint8_t device_addr[6];
  
} mac_addr_t;
mac_addr_t device_list[20];

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
  String payload;
  uint8_t type = Data[0];       // first message byte is the type of message 
  switch (type) 
  {
    case DATA :                           // the message is data type
      memcpy(&DataRecv, Data, sizeof(DataRecv));
      // create a JSON document with received data and send it by event to the web page
      /*root["id"] = DataRecv.Device_Id;
      root["temperature"] = DataRecv.temp;
      root["humidity"] = DataRecv.hum;
      root["ON_OFF"] = DataRecv.on_off;
      serializeJson(root, payload);
      serializeJson(root, Serial);*/
      switch(DataRecv.Device_Type)
      {
        case UMIDADE:
          Serial.println(DataRecv.Device_Id);
          //Serial.println(mac_addr);
          Serial.println(DataRecv.temp);
          break;
        case NIVEL:
          break;
        case TEMPERATURA:
          Serial.println("-----------------------------");
          Serial.print("Dispositivo: ");
          Serial.println(DataRecv.Device_Id);
          Serial.println("Tipo: Temperatura");
          Serial.print("Temperatura Atual: ");
          //Serial.println(mac_addr);
          Serial.print(DataRecv.temp);
          Serial.println("°C");
          Serial.println("-----------------------------");
          break;
      }

      break;
    
    case PAIRING:                            // the message is a pairing request 
      memcpy(&pairingData, Data, sizeof(pairingData));
      if (pairingData.Device_Type != MESTRE) 
      {     // do not replay to server itself
        if (pairingData.msgType == PAIRING) 
        { 
          pairingData.Device_Type = MESTRE;
          // Server is in AP_STA mode: peers need to send data to server soft AP MAC address 
          WiFi.softAPmacAddress(pairingData.macAddr);   
          pairingData.channel = channel;
          esp_now_send(mac_addr, (uint8_t *) &pairingData, sizeof(pairingData));
          addPeer(mac_addr);
        }  
      }  
      break; 
  }
}


void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);
  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }

  channel = WiFi.channel();

  if (esp_now_init() != ESP_OK) 
  {
    esp_restart();
    return;
  }
  Serial.println("Iniciando");
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

}

void loop() {}