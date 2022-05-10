# CS6603 semseter project with title:  Wireless Temperature network using  ESP8266 module and MLX90614 sensor

# Fixed Schedule Code (Fixed Round Robin MAC Protocol)
1.	The programs for each Arduino ESP8266 code correspond to the labeling on the bottom of each breadboard attached to the boards (receiver is the AP board, clients are the Wi-Fi station boards).
2.	Make sure relevant Arduino libraries are downloaded on the device uploading the code to each board.
3.	Enable local hotspot with the same name/password as that defined in the receiver program file, within range of the ESP8266 boards. Information on the web server can only be viewed by devices using the same hotspot.
4.	When the receiver board begins running its code, make sure that the relevant serial monitor is displayed on the device uploading the code. Once a Wi-Fi connection is created, the IP address of the web server will be displayed on the 9600 serial port of the computer.
5.	Enter the IP address as a web address in order to view the web server. ESP8266 boards and sensor will run continuously as long as they have power, and the web sever will update each board’s information at ten second intervals.
6.	Note: Board 1 = Client 1, Board 2 = Client 2, Board 3 = Receiver for temperature information.



# Random Schedule Code (Random Scheduling MAC Protocol)

Mostly the same steps as the code for the fixed scheduling code. Please note that readings of “9999 °C” indicate a transmission collision and are not representative of the actual temperature readings of a temperature sensor at a given point in time.
