#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <EEPROM.h>


#define MAX_CHANNEL 11  // for North America // 13 in Europe

uint8_t Broadcast[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

typedef struct struct_message {
  uint8_t msgType;
  uint8_t Device_Id;
  float temp;
  float hum;
  bool on_off;
} struct_message;

typedef struct struct_pairing {   
    uint8_t msgType;
    uint8_t Device_Id;
    uint8_t macAddr[6];
    uint8_t channel;
} struct_pairing;

struct_message DataSent;
struct_message DataRecv;
struct_pairing pairingData;

enum PairingStatus {NOT_PAIRED, PAIR_REQUEST, PAIR_REQUESTED, PAIR_PAIRED,};
PairingStatus pairingStatus = NOT_PAIRED;

enum MessageType {PAIRING, DATA,};
MessageType messageType;

enum DeviceId {MESTRE, ESCRAVO,};
DeviceId DeviceId = ESCRAVO;

#ifdef SAVE_CHANNEL
  int lastChannel;
#endif  
int channel = 1;
 
unsigned long currentMillis = millis();
unsigned long previousMillis = 0;   // Stores last time temperature was published
const long interval = 10000;        // Interval at which to publish sensor readings
unsigned long start;                // used to measure Pairing time
unsigned int readingId = 0;   


void addPeer(const uint8_t * mac_addr, uint8_t chan){
  esp_now_peer_info_t peer;
  ESP_ERROR_CHECK(esp_wifi_set_channel(chan ,WIFI_SECOND_CHAN_NONE));
  esp_now_del_peer(mac_addr);
  memset(&peer, 0, sizeof(esp_now_peer_info_t));
  peer.channel = chan;
  peer.encrypt = false;
  memcpy(peer.peer_addr, mac_addr, sizeof(uint8_t[6]));
  if (esp_now_add_peer(&peer) != ESP_OK){
    return;
  }
  memcpy(Broadcast, mac_addr, sizeof(uint8_t[6]));
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
}

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *Data, int len) { 
  uint8_t type = Data[0];
  switch (type) 
  {
    case DATA :      // we received data from server
      memcpy(&DataRecv, Data, sizeof(DataRecv));
      break;

    case PAIRING:    // we received pairing data from server
      memcpy(&pairingData, Data, sizeof(pairingData));
      if (pairingData.Device_Id == MESTRE) 
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
    Serial.print("Pairing request on channel "  );
    Serial.println(channel);

    // set WiFi channel   
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel,  WIFI_SECOND_CHAN_NONE));
    if (esp_now_init() != ESP_OK) {
    }

    // set callback routines
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
  
    // set pairing data to send to the server
    pairingData.msgType = PAIRING;
    pairingData.Device_Id = ESCRAVO;    
    pairingData.channel = channel;

    // add peer and send request
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
  pinMode(0, INPUT_PULLUP);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  start = millis();

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
      previousMillis = currentMillis;
      DataSent.msgType = DATA;
      DataSent.Device_Id = ESCRAVO;
      //DataSent.temp = 111.4;
      DataSent.hum = 12.4;
      DataSent.on_off = true;
      esp_err_t result = esp_now_send(Broadcast, (uint8_t *) &DataSent, sizeof(DataSent));
    }
  }
  if(digitalRead(0) == false) DataSent.temp += 10, delay(250);
}