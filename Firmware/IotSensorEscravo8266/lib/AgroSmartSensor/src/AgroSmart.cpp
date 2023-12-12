#include "AgroSmart.h"


uint8_t Broadcast[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

PairingStatus pairingStatus = PAIR_REQUEST;
DeviceType deviceType = NIVEL;
struct_message_t DataSent;
struct_message_t DataRecv;
struct_pairing_t pairingData;

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
    Serial.println("device salvo");
  }
}

void OnDataRecv(uint8_t *mac_addr, uint8_t *Data, uint8_t len) 
{ 
  uint8_t type = Data[0];
  switch (type) 
  {
    case DATA:
      memcpy(&DataRecv, Data, sizeof(DataRecv));
      break;

    case PAIRING:
      memcpy(&pairingData, Data, sizeof(pairingData));
      if (pairingData.Device_Type == MESTRE) 
      {
        EEPROM.write(0, pairingData.channel), EEPROM.commit(); // salva o canal no endereço 0;  
        EEPROM.write(1, 1), EEPROM.commit(); // salva que um despositivo foi salvo no endereço 1;  
        write_mac(pairingData.mac); //salva o mac do dispositivo do endereço 2 até o 7;
        addPeer(pairingData.mac, pairingData.channel); // adiciona o peer
        pairingStatus = PAIR_PAIRED; // define o status como pareado;  
        Serial.println("recebendo dados do mestre");  
      }
      break;
  }  
}

void autoPairing(PairingStatus *status)
{
  static uint8_t channel;
  bool paired = false;
  while(paired == false)
  {
    Serial.println("requisitando conexao");
    init_network_esp(channel);
    pairingData.msgType = PAIRING;
    pairingData.Device_Type = deviceType;    
    esp_now_send(Broadcast, (uint8_t *) &pairingData, sizeof(pairingData));
    pairingStatus = PAIR_REQUESTED;
    delay(channel_update_time);
    if(*status != PAIR_PAIRED)
    {
      channel ++;
      if (channel > MAX_CHANNEL) ESP.restart();
    } 
    else paired = true;
  }
  sensor_function();
}  

void write_mac(uint8_t *mac)
{
  if(EEPROM.read(2) != mac[0]) EEPROM.write(2, mac[0]), EEPROM.commit();
  if(EEPROM.read(3) != mac[1]) EEPROM.write(3, mac[1]), EEPROM.commit();
  if(EEPROM.read(4) != mac[2]) EEPROM.write(4, mac[2]), EEPROM.commit();
  if(EEPROM.read(5) != mac[3]) EEPROM.write(5, mac[3]), EEPROM.commit();
  if(EEPROM.read(6) != mac[4]) EEPROM.write(6, mac[4]), EEPROM.commit();
  if(EEPROM.read(7) != mac[5]) EEPROM.write(7, mac[5]), EEPROM.commit();
}

void get_mac(uint8_t *mac)
{
  mac[0] = EEPROM.read(2);
  mac[1] = EEPROM.read(3);
  mac[2] = EEPROM.read(4);
  mac[3] = EEPROM.read(5);
  mac[4] = EEPROM.read(6);
  mac[5] = EEPROM.read(7);
}

void init_network_esp(uint8_t channel)
{
  esp_now_deinit();
  WiFi.mode(WIFI_STA);
  wifi_promiscuous_enable(1);
  wifi_set_channel(channel);
  wifi_promiscuous_enable(0);
  WiFi.disconnect();
  if (esp_now_init() != ERR_OK) ESP.restart();
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(OnDataRecv);
}

void init_smart_sensor()
{
  EEPROM.begin(10);
  pinMode(0, INPUT_PULLUP);
  delay(5000);
  Serial.println("entrando init");
  if(EEPROM.read(1) == 1 && digitalRead(0) == 1) //se já estiver algum dispositivo adicionado a rede;
  {
    get_mac(Broadcast);
    init_network_esp(EEPROM.read(0));
    esp_now_add_peer(Broadcast, ESP_NOW_ROLE_COMBO, EEPROM.read(0), NULL, 0);
    sensor_function();
  }
  else autoPairing(&pairingStatus);

}

void sensor_function()
{
    DataSent.msgType = DATA;
    DataSent.Device_Type = deviceType;
    DataSent.temp = random(100);

    esp_now_send(Broadcast, (uint8_t *) &DataSent, sizeof(DataSent));
    ESP.deepSleep(60e6);
}