

#include <StopWatch.h>

#include <ESP8266WiFi.h>
#include <espnow.h>

#include "ESPAsyncWebServer.h"
#include "ESPAsyncTCP.h"
#include <Arduino_JSON.h>

#include <Adafruit_MLX90614.h>

#include <Wire.h>

 
 
//The id of this Arduino WiFi board and the time variable used to sync the time/schedules of the other sensors.
#define BOARD_ID 3
#define TIME_LINEUP 0


//Stopwatch used to keep track of transmission timer.
//This is used instead of millis() because resetting millis() is not easy and potentially dangerous to do.
StopWatch synctimer;
StopWatch SWarray[5];


//Initiate temperature sensor.
Adafruit_MLX90614 mlx = Adafruit_MLX90614();


//The MAC addresses of the devices within the wireless network.
uint8_t client_one_address[] = {0x40, 0x91, 0x51, 0x4F, 0x42, 0x00};

uint8_t client_two_address[] = {0x40, 0x91, 0x51, 0x50, 0x15, 0x55};

uint8_t this_board_address[] = {0x4C, 0x75, 0x25, 0x35, 0x1D, 0x35};


//The number of collisions that have occurred during transmission.
int collision_count = 0;


//Stores the arrival times of data transmissions.
unsigned long arrival_times[4] {0, 9999, 99999, 7500};

//Stoes the ids for the boards that transmissions have caused a conflict.
int board_ids_recieved[4] {0, 0, 0, 0};


unsigned long board_one_arrival_time = 0;
unsigned long board_two_arrival_time = 0;

bool transmission_collide_with_ap = false;

/*
arrival_times[0] = 0;

arrival_times[3] = 7500;

//Default values if no transmission is recieved to avoid these values counting as a transmission collision.
arrival_times[1] = 9999;

arrival_times[2] = 99999;
*/

unsigned long time_difference = 0;

//The network information for my phone's hotspot.
const char* ssid = "write your wifo ssid"; 
const char* password = "write your wifo password";


//The intended time between transmissions to the other sensors and the network and the last time a transmission was sent.
const long transmission_interval = 7500; 
unsigned long previous_time = 0;    


//Structure of relevant temperature sensor information. This will also be the data structure the client sensors send to this server sensor.
typedef struct temperature_message {
  int id;
  float object_temp;
  float ambient_temp;
  unsigned int when_read_id;
} temperature_message;

//Data recieved from the other boards.
temperature_message recieved_data;

//The temperature (and other data) from this board sensor.
temperature_message this_board_data;

temperature_message crash_count_data;


//Data strucutre for information sent to client boards.
//It is intended to sync the timers/ schedules between the boards.
typedef struct time_sync_message {
    int id;
    int timesync;
} time_sync_message;


//Get object temperature reading from the sensor.
float getobjtemp() {
  float ot = mlx.readObjectTempC();
  return ot;
}

//Get ambient temperature reading from the sensor.
float readambtemp() {
  float at = mlx.readAmbientTempC();
  return at;
}



time_sync_message timesend_one;
time_sync_message timesend_two;


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



//Create board to display JSON information and send it to an html to see the information.
JSONVar board;

//Create web server to dsiplay infromation on phone.
//Only works for devices connected to the hotspot.
AsyncWebServer server(80);
AsyncEventSource events("/events");

//Called whenever a transmission is recieved from the other boards.
//Prints not necessary, but are useful for debugging and to show how the results are recieved
//prior to being sent to the webserver.
void when_transmission_recieved(uint8_t * recieved_mac_address, uint8_t *data_being_sent, uint8_t transmission_length) { 
  
  //Prints the MAC address of the board whihc sent the transmission..
  char macaddr[18];
  Serial.print("Packet received from: ");
  snprintf(macaddr, sizeof(macaddr), "%02x:%02x:%02x:%02x:%02x:%02x",
           recieved_mac_address[0], recieved_mac_address[1], recieved_mac_address[2], recieved_mac_address[3], recieved_mac_address[4], recieved_mac_address[5]);
  Serial.println(macaddr);

  //Maps the recieved transmission data to the receieved_data variable.
  memcpy(&recieved_data, data_being_sent, sizeof(recieved_data));


  if (recieved_data.id == 1) {
    arrival_times[1] = synctimer.elapsed();
  }

  if (recieved_data.id == 2) {
    arrival_times[2] = synctimer.elapsed();
  }

  for (int i=0; i<3; i++) {
      for (int j=i+1; j<4; j++) {
        time_difference = arrival_times[i] - arrival_times[j];
        //Serial.println(time_difference);
        if ((time_difference >= 4294966294) || (time_difference <= 1000)){
          collision_count = collision_count + 1;
          board_ids_recieved[i] = 1;
          board_ids_recieved[j] = 1;
        }
      }
    }

  bool found_conflict_in_reception = false;
  for (int k=0; k<4; k++) {
      if (board_ids_recieved[k] == 1) {
        if ((k == 0) || (k == 3))
        {
          transmission_collide_with_ap = true;
          found_conflict_in_reception;
          /*
          board["id"] = 3;
          board["ambienttemperature"] = 9999;
          board["objtemperature"] = 9999;
          board["when_read_id"] = String(9999);
          String jsonString = JSON.stringify(board);
          events.send(jsonString.c_str(), "new_data", current_time);
          */
        }
        if (k == 1)
        {
          board["id"] = 1;
          board["ambienttemperature"] = 9999;
          board["objtemperature"] = 9999;
          board["when_read_id"] = String(9999);
          String jsonString = JSON.stringify(board);
          events.send(jsonString.c_str(), "new_data", arrival_times[1]);
          found_conflict_in_reception = true;
        }
        if (k == 2)
        {
          board["id"] = 2;
          board["ambienttemperature"] = 9999;
          board["objtemperature"] = 9999;
          board["when_read_id"] = String(9999);
          String jsonString = JSON.stringify(board);
          events.send(jsonString.c_str(), "new_data", arrival_times[2]);
          found_conflict_in_reception = true;
        }
      }
    }

  if (found_conflict_in_reception == false) {
    
    //Send the relevent information to the JSON board and update the infromation to be updated to the web server.
    board["id"] = recieved_data.id;
    board["objtemperature"] = recieved_data.object_temp;
    board["ambienttemperature"] = recieved_data.ambient_temp;
    board["when_read_id"] = String(recieved_data.when_read_id);
    String jsonString = JSON.stringify(board);
    events.send(jsonString.c_str(), "new_data", millis());
  }
  else
  {
    found_conflict_in_reception = false;
  }


  //Prints the recieved transmission data. Will display on the specified serial port.
  Serial.println("Board ID: ");
  Serial.println(recieved_data.id);
  Serial.println("objecttemp value: ");
  Serial.println(recieved_data.object_temp);
  Serial.println("Ambienttemp value: ");
  Serial.println(recieved_data.ambient_temp);
  Serial.println("readingID value: ");
  Serial.println(recieved_data.when_read_id);
  Serial.println();

  




  
}


//Code to create the HTML web server and to specify display information. 
//Not particularly relevant to the workings of the wireless network itself, but rather for sending and displaying the
//gathered infromation on the phone.
//The reicved date/time is also displayed, althouth this information is also displayed, to show the staggering of transmission times.
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP-NOW DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h1 {  font-size: 2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #2f4468; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .timestamp { color: #bebebe; font-size: 1rem; }
    .card-title{ font-size: 1.2rem; font-weight : bold; }
    .card.objtemperature { color: #B10F2E ; }
    .card.ambienttemperature { color: #248a24; }
    .card.collision {color: #0f35b1;}
  </style>
</head>
<body>
  <div class="topnav">
    <h1>Wireless Temperature Monitoring Using MAC Protocol </h1>
  </div>
  <div class="content">
    <div class="cards">
       <div class="card collision">
        <p class="card-title"><i class="fas fa-explosion"></i>  COLLISION COUNT</p><p><span class="reading"><span id="t4"></span> </span></p><p class="timestamp">Last Reading: <span id="rt4"></span></p>
      </div>
      <div class="card objtemperature">
        <p class="card-title"><i class="fas fa-thermometer-half"></i> BOARD #1 - OBJECT TEMPERATURE</p><p><span class="reading"><span id="t1"></span> &deg;C</span></p><p class="timestamp">Last Reading: <span id="rt1"></span></p>
      </div>
      <div class="card ambienttemperature">
        <p class="card-title"><i class="fas fa-thermometer-half"></i> BOARD #1 - AMBIENT TEMPERATURE</p><p><span class="reading"><span id="h1"></span> &deg;C</span></p><p class="timestamp">Last Reading: <span id="rh1"></span></p>
      </div>
      <div class="card objtemperature">
        <p class="card-title"><i class="fas fa-thermometer-half"></i> BOARD #2 - OBJECT TEMPERATURE</p><p><span class="reading"><span id="t2"></span> &deg;C</span></p><p class="timestamp">Last Reading: <span id="rt2"></span></p>
      </div>
      <div class="card ambienttemperature">
        <p class="card-title"><i class="fas fa-thermometer-half"></i> BOARD #2 - AMBIENT TEMPERATURE</p><p><span class="reading"><span id="h2"></span> &deg;C</span></p><p class="timestamp">Last Reading: <span id="rh2"></span></p>
      </div>
      <div class="card objtemperature">
        <p class="card-title"><i class="fas fa-thermometer-half"></i> BOARD #3 - OBJECT TEMPERATURE</p><p><span class="reading"><span id="t3"></span> &deg;C</span></p><p class="timestamp">Last Reading: <span id="rt3"></span></p>
      </div>
      <div class="card ambienttemperature">
        <p class="card-title"><i class="fas fa-thermometer-half"></i> BOARD #3 - AMBIENT TEMPERATURE</p><p><span class="reading"><span id="h3"></span> &deg;C</span></p><p class="timestamp">Last Reading: <span id="rh3"></span></p>



        
      </div>
    </div>
  </div>
<script>
function getDateTime() {
  var currentdate = new Date();
  var datetime = currentdate.getDate() + "/"
  + (currentdate.getMonth()+1) + "/"
  + currentdate.getFullYear() + " at "
  + currentdate.getHours() + ":"
  + currentdate.getMinutes() + ":"
  + currentdate.getSeconds();
  return datetime;
}
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
 
 source.addEventListener('new_data', function(e) {
  console.log("new_data", e.data);
  var obj = JSON.parse(e.data);
  document.getElementById("t"+obj.id).innerHTML = obj.objtemperature.toFixed(2);
  document.getElementById("h"+obj.id).innerHTML = obj.ambienttemperature.toFixed(2);
  document.getElementById("rt"+obj.id).innerHTML = getDateTime();
  document.getElementById("rh"+obj.id).innerHTML = getDateTime();
 }, false);
}
</script>
</body>
</html>)rawliteral";

void setup() {
  //Initialize serial monitor on which to display information. (Mostly useful for seeeing data or debuggingwhen connected to a terminal such as a computer).
  Serial.begin(9600);

  //Sets this Arduino boards as both an Access Point and an Station.
  //This board serves as the AP for other boards to connect to the phone's hotspot.
  //(AP sends the other sensor's data to the web server).
  //This board also needs to be specified as a Station so that it can send transmissions to other boards
  //being a Station is not necessary to to recieve transmissions).
  WiFi.mode(WIFI_AP_STA);


  

  //Initialize temperature sensor.
  mlx.begin(); 

  


  //Set device as a WiFi Station and generate the IP address to connect to to display the infrormation/ web server.
  //A fiex IP address could be used, but it was decided to generate a local one in case other groups
  //happen to use the same IP address (which can happen).
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  //Initialize ESP-NOW, which is the library used to set up most of the WiFi connections in this program.
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }


  
  //Set ESP-NOW role. Role is set to "combo" since this device both sends and recieves tranmissions.
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

  //Set when_transmission_sent function to execute upon recieving a transmission.
  esp_now_register_send_cb(when_transmission_sent);
  
  //Register other sensors within the network.
  esp_now_add_peer(client_one_address, ESP_NOW_ROLE_COMBO, 1, NULL, 0);

  esp_now_add_peer(client_two_address, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  
  //Set when_transmission_recieved function to execute upon recieving a transmission.
  esp_now_register_recv_cb(when_transmission_recieved);




  
  //Initialize web server.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!" and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 2000);
  });
  server.addHandler(&events);
  server.begin();

  //Start stopwatch timer.
  synctimer.start();
  

  //The chosen mlx temperature sensors generate incorrect readings if not given enough time to calibrate to the ESP8266 WiFi board.
  //This is due to an I2C clocking rate difference/error. This can often be fixed by setting the clock speed for a program, but 
  //due to the asynchrounous WiFi nature of this board this is ineffective.
  //This temperature reading issue only occurs for the object temperature, not the ambient temperature, and does not occur if 
  //the information is displayed on a serial port instead, if a large enough delay is given for the temperature sensor to reset.
  //Wire.begin();
  //Wire.setClock(100000L);
  //Wire.setClock(400000L);
}

unsigned int myreadingId = 0;
unsigned int crashreadingId = 0;





void loop() {
  unsigned long current_time = millis();

  //Send a transmission and read the temperature data every time interval. In this case, every 7.4 seconds.
  if (current_time - previous_time >= transmission_interval) {

    //Update the last time a transmission was sent.
    previous_time = current_time;

    

    //Set values to send
    timesend_one.id = BOARD_ID;
    timesend_two.id = BOARD_ID;
    timesend_one.timesync = random(1000, 7500);
    timesend_two.timesync = random(1000, 7500);

    //Get temperature sensor readings for this board.
    this_board_data.id = BOARD_ID;

    //The temperature sensor takes some time to recalibrate, so the first reading (object or ambitent)
    //has to be called twice for it to display correctly.
    this_board_data.object_temp = getobjtemp();
    this_board_data.object_temp = getobjtemp();
    this_board_data.ambient_temp = readambtemp();
    this_board_data.when_read_id = myreadingId++;

    if (transmission_collide_with_ap == false) {
      
      //Send this board's temperature sensor data to the JSON/ web server.
      board["id"] = this_board_data.id;
      board["ambienttemperature"] = this_board_data.ambient_temp;
      board["objtemperature"] = this_board_data.object_temp;
      board["when_read_id"] = String(this_board_data.when_read_id);
      String jsonString = JSON.stringify(board);
      events.send(jsonString.c_str(), "new_data", millis());
    }
    else {
      board["id"] = 3;
      board["ambienttemperature"] = 9999;
      board["objtemperature"] = 9999;
      board["when_read_id"] = String(9999);
      String jsonString = JSON.stringify(board);
      events.send(jsonString.c_str(), "new_data", current_time);
      transmission_collide_with_ap = false;
      //Serial.println("////////////////////////////////////////////////////////////////////////");
    }

    /*
    for (int k=0; k<4; k++) {
      Serial.println(arrival_times[k]);
    }
    */

    /*
    //Maximum unsiged long value is 4,294,967,295.
    
    for (int i=0; i<3; i++) {
      for (int j=i+1; j<4; j++) {
        time_difference = arrival_times[i] - arrival_times[j];
        //Serial.println(time_difference);
        if ((time_difference >= 4294966294) || (time_difference <= 1000)){
          collision_count = collision_count + 1;
          board_ids_recieved[i] = 1;
          board_ids_recieved[j] = 1;
        }
      }
    }
    */


    
    crash_count_data.id = 4;

    crash_count_data.object_temp = collision_count;
    crash_count_data.ambient_temp = 1;
    crash_count_data.when_read_id = crashreadingId++;

    //Send this board's temperature sensor data to the JSON/ web server.
    board["id"] = crash_count_data.id;
    board["ambienttemperature"] = crash_count_data.ambient_temp;
    board["objtemperature"] = crash_count_data.object_temp;
    board["when_read_id"] = String(crash_count_data.when_read_id);
    String jsonStringtwo = JSON.stringify(board);
    events.send(jsonStringtwo.c_str(), "new_data", millis());
    

    Serial.println();

    
    for (int k=0; k<4; k++) {
      Serial.println(board_ids_recieved[k]);
    }
    
    
    Serial.println("Crash Count: ");
    Serial.println(collision_count);
    Serial.println();

    //Default values if no transmission is recieved to avoid these values counting as a transmission collision.
    arrival_times[1] = 9999;
    
    arrival_times[2] = 99999;

    for (int k=0; k<4; k++) {
      board_ids_recieved[k] = 0;
    }

    
    synctimer.reset();
    synctimer.start();

    //Send time sync message to client boards.
    esp_now_send(client_one_address, (uint8_t *) &timesend_one, sizeof(timesend_one));

    esp_now_send(client_two_address, (uint8_t *) &timesend_two, sizeof(timesend_two));

    

    
  }
}
