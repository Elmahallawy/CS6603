
#include <StopWatch.h>

#include <ESP8266WiFi.h>
#include <espnow.h>

//#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

#include <Wire.h>
#include <Adafruit_MLX90614.h>

// Import required libraries
#include <Arduino.h>

#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>



//Designate Arduino WiFi board id for wireless network communication.
#define BOARD_ID 1

//Initialize temperature sensor
Adafruit_MLX90614 mlx = Adafruit_MLX90614();


//Stopwatch used to keep track of transmission timer.
//This is used instead of millis() because resetting millis() is not easy and potentially dangerous to do.
StopWatch synctimer;
StopWatch SWarray[5];

bool recieved_start_signal = false;

//MAC address of the AP board.
uint8_t ap_mac_address[] = {0x4C, 0x75, 0x25, 0x35, 0x1D, 0x35};

//Data type for the transmssion data being sent to the AP.
typedef struct transmission_data {
    int id;
    float temp;
    float hum;
    int readingId;
} transmission_data;


transmission_data this_boards_data;



unsigned int myreadingId = 0;

//The WiFi network's SSID.
constexpr char WIFI_SSID[] = "projectwlsn";

int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
    for (uint8_t i=0; i<n; i++) {
      if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
        return WiFi.channel(i);
      }
    }
  }
  return 0;
}


//Get object temperature reading from the sensor.
float getobjtemperature() {
  //float t = bme.getobjtemperature();
  float ot =mlx.readObjectTempC();
  return ot;
}


//Get ambient temperature reading from the sensor.
float getambienttemperature() {
  //float h = bme.getambienttemperature();
  float at = mlx.readAmbientTempC();
  return at;
}



//Called whenever the board sends a transmission.
//Prints not really needed, but used for debugging and to show how the network transmissions operate.
void when_transmission_sent(uint8_t *recieved_mac_address, uint8_t delivery_successful) {
  Serial.print("Last transmission status: ");

  //Prints the MAC adress of the intended reciever.
  char macaddr[18];
  Serial.print("Transmission sent to: ");
  snprintf(macaddr, sizeof(macaddr), "%02x:%02x:%02x:%02x:%02x:%02x",
           recieved_mac_address[0], recieved_mac_address[1], recieved_mac_address[2], recieved_mac_address[3], recieved_mac_address[4], recieved_mac_address[5]);
  Serial.println(macaddr);
  
  //Prints status of the transmission delivery.
  if (delivery_successful == 0){
    Serial.println("Success");
  }
  
  else{
    Serial.println("Failure");
  }
}



//DAta structure for the transmission expected to be recieved by the AP board.
typedef struct recieve_message {
    int id;
    int timesync;
} recieve_message;


recieve_message recieved_data;


//Variables to store the data being sent to this board.
int incomingid;
int incomingtimesync;

//Updates sensor readings and sends a transmission every 7.5 seconds.
unsigned long interval = 7500; 

//The last time a transmission was sent and the current time.
unsigned long previous_time = 0;   
unsigned long current_time = millis();

//Variable used to sync this board's time with the cluster head/ AP board's timer.
unsigned long inc_time = 0; 



//Called whenever a transmission is recieved from the other boards.
//Prints not necessary, but are useful for debugging and to show how the results are recieved
//prior to being sent to the webserver.
void when_transmission_recieved(uint8_t * sent_mac_address, uint8_t *data_being_sent, uint8_t transmission_length) {
  memcpy(&recieved_data, data_being_sent, sizeof(recieved_data));

  
  incomingid = recieved_data.id;
  Serial.println("Recieved from: ");
  Serial.println(recieved_data.id);

  
  incomingtimesync = recieved_data.timesync;
  Serial.println("Time sync:  ");
  Serial.println(recieved_data.timesync);

  recieved_start_signal = true;
  
  //Sync this board's timer with that of the AP board.
  //inc_time = 2500;
  interval = recieved_data.timesync;
  previous_time = 0;

  synctimer.reset();
  synctimer.start();
  
}







 
void setup() {
  //Initialize serial monitor on which to display information. (Mostly useful for seeeing data or debuggingwhen connected to a terminal such as a computer).
  Serial.begin(9600);
   
  //Initialize temperature sensor.
  mlx.begin();  

  //Set this board as a WiFi station.
  WiFi.mode(WIFI_STA);

  //Set the WiFi channel for this board.
  int32_t channel = getWiFiChannel(WIFI_SSID);

  //Prints used to verify channel number (not neded but useful for debugging and to see the program setup).
  WiFi.printDiag(Serial); 
  wifi_promiscuous_enable(1);
  wifi_set_channel(channel);
  wifi_promiscuous_enable(0);
  WiFi.printDiag(Serial);

  //Initialize ESP-NOW, which is the library used to set up most of the WiFi connections in this program.
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  
  //Set ESP-NOW role. Role is set to "combo" since this device both sends and recieves tranmissions.
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

  //Set when_transmission_sent function to execute upon recieving a transmission.
  esp_now_register_send_cb(when_transmission_sent);
  
  //Register AP node to send transmissions to within the wireless network.
  esp_now_add_peer(ap_mac_address, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  
  //Set when_transmission_recieved function to execute upon recieving a transmission.
  esp_now_register_recv_cb(when_transmission_recieved);

  //Start stopwatch timer.
  synctimer.start();
}
 
void loop() {

  if (recieved_start_signal == true) {
    
  
    current_time = inc_time + synctimer.elapsed();
    if (current_time - previous_time >= interval) {
      
      //Reset timer and assoicated variables.
      inc_time = 0;
      
      synctimer.reset();
      synctimer.start();
  
      
      //Get transmission variables and temperature sesnor readings.
      this_boards_data.id = BOARD_ID;
      this_boards_data.temp = getobjtemperature();
      this_boards_data.hum = getambienttemperature();
      this_boards_data.readingId = myreadingId++;
  
  
      
  
  
  
      //Send data to AP board.
      esp_now_send(ap_mac_address, (uint8_t *) &this_boards_data, sizeof(this_boards_data));

      //Used to stop the board from sending multiple signals during the same schedule interval.
      recieved_start_signal = false;
  
    }

    
  }
}
