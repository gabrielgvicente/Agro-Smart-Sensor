#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <EEPROM.h>


#define MAX_CHANNEL 11

uint8_t Broadcast[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

enum PairingStatus {NOT_PAIRED, PAIR_REQUEST, PAIR_REQUESTED, PAIR_PAIRED,};
PairingStatus pairingStatus = NOT_PAIRED;

enum MessageType {PAIRING, DATA,};
MessageType messageType;

enum DeviceType {MESTRE, UMIDADE, TEMPERATURA, NIVEL};
DeviceType DeviceType = UMIDADE;

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
bool led_state_ant = false; 

void addPeer(const uint8_t *mac_addr, uint8_t chan)
{
  esp_now_peer_info_t peer;
  ESP_ERROR_CHECK(esp_wifi_set_channel(chan ,WIFI_SECOND_CHAN_NONE));
  esp_now_del_peer(mac_addr);
  memset(&peer, 0, sizeof(esp_now_peer_info_t));
  peer.channel = chan;
  peer.encrypt = 0;
  memcpy(peer.peer_addr, mac_addr, sizeof(uint8_t [6]));
  esp_err_t addStatus = esp_now_add_peer(&peer);
  if (addStatus == ESP_OK) {
    Serial.println("Pareado");
  }
  else 
  {
    Serial.println("Falha ao parear");
  }
  memcpy(Broadcast, mac_addr, sizeof(uint8_t[6]));
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
}

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *Data, int len) { 
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
      // set WiFi channel   
      ESP_ERROR_CHECK(esp_wifi_set_channel(channel,  WIFI_SECOND_CHAN_NONE));
      if (esp_now_init() != ESP_OK) {
      }
      esp_now_register_send_cb(OnDataSent);
      esp_now_register_recv_cb(OnDataRecv);
      pairingData.msgType = PAIRING;
      pairingData.Device_Type = DeviceType;    
      pairingData.channel = channel;
      addPeer(Broadcast, channel);
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

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

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
      // Save the last time a new reading was published

      if(led_state_ant == true) digitalWrite(LED_BUILTIN, false), led_state_ant = false;
      else digitalWrite(LED_BUILTIN, true), led_state_ant = true;
      previousMillis = currentMillis;
      DataSent.msgType = DATA;
      DataSent.Device_Type = DeviceType;
      DataSent.hum = random(100);
      esp_now_send(Broadcast, (uint8_t *) &DataSent, sizeof(DataSent));
    }
  }
}