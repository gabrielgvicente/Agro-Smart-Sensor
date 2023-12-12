#ifndef AgroSmart_h
#define AgroSmart_h

#include <Arduino.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>

#define MAX_CHANNEL 11
#define channel_update_time 300

enum PairingStatus {PAIR_REQUEST, PAIR_REQUESTED, PAIR_PAIRED};

enum MessageType {PAIRING, DATA};

enum DeviceType {MESTRE, UMIDADE, TEMPERATURA, NIVEL};

typedef struct {
  uint8_t msgType;
  uint8_t Device_Type;
  float temp;
  float hum;
  bool on_off;
} struct_message_t;

typedef struct {   
    uint8_t msgType;
    uint8_t Device_Type;
    uint8_t mac[6];
    uint8_t channel;
} struct_pairing_t;


void addPeer(uint8_t *mac_addr, uint8_t chan);

void OnDataRecv(uint8_t *mac_addr, uint8_t *Data, uint8_t len);

void autoPairing(PairingStatus status);

void write_mac(uint8_t *mac);

void get_mac(uint8_t *mac);

void init_network_esp(uint8_t channel);

void init_smart_sensor();

void sensor_function();

#endif
