//#include <Arduino.h>
#include <espnow.h>
//#include <esp_wifi.h>
#include <ESP8266WiFi.h>
//#include <WiFi.h>
#include <EEPROM.h>


#define MAX_CHANNEL 11

uint8_t Broadcast[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

enum PairingStatus {NOT_PAIRED, PAIR_REQUEST, PAIR_REQUESTED, PAIR_PAIRED,};
PairingStatus pairingStatus = NOT_PAIRED;

enum MessageType {PAIRING, DATA,};
MessageType messageType;

enum DeviceType {MESTRE, UMIDADE, TEMPERATURA, NIVEL};
DeviceType DeviceType = TEMPERATURA;

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

#ifdef SAVE_CHANNEL
  int lastChannel;
#endif  
uint8_t channel = 1;
unsigned long currentMillis = millis();
unsigned long previousMillis = 0;   // Stores last time temperature was published
const long interval = 30000;        // Interval at which to publish sensor readings

void addPeer(uint8_t *mac_addr, uint8_t chan)
{
  bool exists = esp_now_is_peer_exist(mac_addr);
  if (exists) 
  {
    Serial.println("Dispositivo já pareado");
  }
  else
  {
    esp_now_del_peer(mac_addr);
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_add_peer(mac_addr, ESP_NOW_ROLE_COMBO, chan, NULL, 0);
    memcpy(Broadcast, mac_addr, sizeof(uint8_t[6]));
    Serial.println("Dispositivo cadastrado");
  }
}

void OnDataRecv(uint8_t * mac_addr, uint8_t *Data, uint8_t len) { 
  uint8_t type = Data[0];
  switch (type) 
  {
    case DATA:
      memcpy(&DataRecv, Data, sizeof(DataRecv));
      break;

    case PAIRING:    // we received pairing data from server
      memcpy(&pairingData, Data, sizeof(pairingData));
      if (pairingData.Device_Type == MESTRE) 
      { 
        addPeer(pairingData.macAddr, pairingData.channel); // add the server  to the peer list 
        #ifdef SAVE_CHANNEL
          lastChannel = pairingData.channel;
          EEPROM.write(0, pairingData.channel);
          EEPROM.commit();
        #endif  
        pairingStatus = PAIR_PAIRED;             // set the pairing status
      }
      break;
  }  
}

PairingStatus autoPairing(){
  switch(pairingStatus) 
  {
    case PAIR_REQUEST:
      esp_now_deinit();
      WiFi.mode(WIFI_STA);
      wifi_promiscuous_enable(1);
      wifi_set_channel(channel);
      wifi_promiscuous_enable(0);
      WiFi.disconnect();
      if (esp_now_init() != ERR_OK) ESP.restart();
      esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
      esp_now_register_recv_cb(OnDataRecv);
      pairingData.msgType = PAIRING;
      pairingData.Device_Type = DeviceType;    
      esp_now_send(Broadcast, (uint8_t *) &pairingData, sizeof(pairingData));
      previousMillis = millis();
      pairingStatus = PAIR_REQUESTED;
    break;

    case PAIR_REQUESTED:
      // time out to allow receiving response from server
      currentMillis = millis();
      if(currentMillis - previousMillis > 250) {
        previousMillis = currentMillis;
        // time out expired,  try next channel
        channel ++;
        if (channel > MAX_CHANNEL){
          channel = 1;
        }   
        pairingStatus = PAIR_REQUEST;
      }
    break;

    case PAIR_PAIRED:
      Serial.println("pareado");

    break;
  }
  return pairingStatus;
}  

void setup() 
{
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ERR_OK) ESP.restart();
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(OnDataRecv);
  #ifdef SAVE_CHANNEL 
    EEPROM.begin(10);
    lastChannel = EEPROM.read(0);
    if (lastChannel >= 1 && lastChannel <= MAX_CHANNEL) {
      channel = lastChannel; 
    }
  #endif 
  pairingStatus = PAIR_REQUEST;
}  

void loop() 
{
  if (autoPairing() == PAIR_PAIRED) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) 
    {
      previousMillis = currentMillis;
      DataSent.msgType = DATA;
      DataSent.Device_Type = DeviceType;
      DataSent.temp = random(100);
      esp_now_send(Broadcast, (uint8_t *) &DataSent, sizeof(DataSent));
    }

  }
}