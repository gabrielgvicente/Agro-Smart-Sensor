#include <esp_now.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "AsyncTCP.h"
#include <ArduinoJson.h>

// Replace with your network credentials (STATION)
const char* ssid = "iPhone de Gabriel";
const char* password = "12345678";

esp_now_peer_info_t slave;
int chan; 

int counter = 0;

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

enum MessageType {PAIRING, DATA,};
MessageType messageType;

enum DeviceId {MESTRE, ESCRAVO,};
DeviceId DeviceId = MESTRE;

AsyncWebServer server(80);
AsyncEventSource events("/events");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP-NOW DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #2f4468; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .packet { color: #bebebe; }
    .card.temperature { color: #fd7e14; }
    .card.humidity { color: #1b78e2; }
  </style>
</head>
<body>
  <div class="topnav">
    <h3>ESP-NOW DASHBOARD</h3>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> BOARD #1 - TEMPERATURE</h4><p><span class="reading"><span id="t1"></span> &deg;C</span></p><p class="packet">Reading ID: <span id="rt1"></span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> BOARD #1 - HUMIDITY</h4><p><span class="reading"><span id="h1"></span> &percnt;</span></p><p class="packet">Reading ID: <span id="rh1"></span></p>
      </div>
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> BOARD #2 - TEMPERATURE</h4><p><span class="reading"><span id="t2"></span> &deg;C</span></p><p class="packet">Reading ID: <span id="rt2"></span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> BOARD #2 - HUMIDITY</h4><p><span class="reading"><span id="h2"></span> &percnt;</span></p><p class="packet">Reading ID: <span id="rh2"></span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('new_readings', function(e) {
  console.log("new_readings", e.data);
  var obj = JSON.parse(e.data);
  document.getElementById("t"+obj.id).innerHTML = obj.temperature.toFixed(2);
  document.getElementById("h"+obj.id).innerHTML = obj.humidity.toFixed(2);
  document.getElementById("rt"+obj.id).innerHTML = obj.readingId;
  document.getElementById("rh"+obj.id).innerHTML = obj.readingId;
 }, false);
}
</script>
</body>
</html>)rawliteral";

bool addPeer(const uint8_t *peer_addr) {      // add pairing
  memset(&slave, 0, sizeof(slave));
  const esp_now_peer_info_t *peer = &slave;
  memcpy(slave.peer_addr, peer_addr, 6);
  
  slave.channel = chan; // pick a channel
  slave.encrypt = 0; // no encryption
  // check if the peer exists
  bool exists = esp_now_is_peer_exist(slave.peer_addr);
  if (exists) {
    // Slave already paired.
    Serial.println("Already Paired");
    return true;
  }
  else {
    esp_err_t addStatus = esp_now_add_peer(peer);
    if (addStatus == ESP_OK) {
      // Pair success
      Serial.println("Pair success");
      return true;
    }
    else 
    {
      Serial.println("Pair failed");
      return false;
    }
  }
} 

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
}

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *Data, int len) { 
  StaticJsonDocument<1000> root;
  String payload;
  uint8_t type = Data[0];       // first message byte is the type of message 
  switch (type) 
  {
    case DATA :                           // the message is data type
      memcpy(&DataRecv, Data, sizeof(DataRecv));
      // create a JSON document with received data and send it by event to the web page
      root["id"] = DataRecv.Device_Id;
      root["temperature"] = DataRecv.temp;
      root["humidity"] = DataRecv.hum;
      root["ON_OFF"] = DataRecv.on_off;;
      serializeJson(root, payload);
      serializeJson(root, Serial);
      events.send(payload.c_str(), "new_readings", millis());
      break;
    
    case PAIRING:                            // the message is a pairing request 
      memcpy(&pairingData, Data, sizeof(pairingData));
      if (pairingData.Device_Id == ESCRAVO) 
      {     // do not replay to server itself
        if (pairingData.msgType == PAIRING) 
        { 
          pairingData.Device_Id = MESTRE;       // 0 is server
          // Server is in AP_STA mode: peers need to send data to server soft AP MAC address 
          WiFi.softAPmacAddress(pairingData.macAddr);   
          pairingData.channel = chan;
          esp_err_t result = esp_now_send(mac_addr, (uint8_t *) &pairingData, sizeof(pairingData));
          addPeer(mac_addr);
        }  
      }  
      break; 
  }
}

void initESP_NOW(){
    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
      esp_restart();
      return;
    }
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
} 

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  Serial.println();
  Serial.print("Server MAC Address:  ");
  Serial.println(WiFi.macAddress());

  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);
  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }

  Serial.print("Server SOFT AP MAC Address:  ");
  Serial.println(WiFi.softAPmacAddress());

  chan = WiFi.channel();
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  initESP_NOW();
  
  // Start Web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  

  // Events 
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  
  // start server
  server.begin();

}

void loop() {
  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 5000;
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
    events.send("ping",NULL,millis());
    lastEventTime = millis();
  }
}