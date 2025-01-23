# ESP32-Based-APRS-Weather-Station
This project implements APRS-IS (Automatic Packet Reporting System-Internet Service) and the MQTT protocol using an ESP32 board. The purpose of this project is to transmit measured weather parameters through these protocols. Before we go to the reporting system, let's talk about the hardware first.
# HARDWARE
So, there are at least 4 parameters measured by this system with nodemcu ESP32 dev module as the main board. Those parameters including temperature, humidity, pressure and irradiance. The temperature and relative humidity are measured with a DHT11 sensor, air pressure with a BMP280 sensor (althought this sensor also could do temperature measurement), and solar irradiance with a BH1750 sensor. The system is also equipped with an RTC DS1302 module for NTP time backup and an active buzzer for notifications when the package uploaded. All components then mounted to a 5 x 7 cm holed empty universal pcb, make it small and compact.

Here are the components list i used for this kit :
- NodeMCU ESP32 DEV KIT;
- 5 x 7 cm holed empty universal pcb;
- DHT11 sensor;
- BMP280 sensor;
- BH1750 sensor;
- Active Buzzer;
- some jumper wires.


