# WIoT Final Project - Group 15

This repository contains code for a transit tracking application. Specifically:
 - The `server` directory reads data from a [`NEO-6M` GPS module](https://www.amazon.com/Microcontroller-Compatible-Sensitivity-Navigation-Positioning/dp/B07P8YMVNT?th=1) to a [`Heltec WiFi LoRa 32 (V3)`](https://heltec.org/project/wifi-lora-32-v3/) board, then transmits that data to [The Things Network (TTN)](https://www.thethingsnetwork.org/) over [LoRaWAN](https://lora-alliance.org/about-lorawan/). 
 - The `client` directory pulls data from TTN via [MQTT](https://mqtt.org/), then loads a dynamic [Flask](https://flask.palletsprojects.com/en/2.3.x/) web server that can be used to view/track bus locations. Note that this uses the [Google Maps API](https://developers.google.com/maps).

<div align="center">
    <img src="https://github.com/ncooney/transit-tracker/blob/main/client/static/architecture.png" alt="System Architecture">
</div>

## Setup

Update `client/config.ini` with your TTN MQTT connection information and your Google API key.

Update the `appEui`, `devEui`, and `appKey` in `server/src/main.cpp` with your TTN device information.

## How to Run (Client)

To run the **full client**, execute `./client.sh` in the `client/src` directory.
 - This runs both the MQTT decoder and Flask server in the background
 - This hides standard output
 - The Flask server can be accessed at [`http://127.0.0.1:5000/`](http://127.0.0.1:5000/)
 - Execute `./client.sh -k` to kill these processes

To run only the **MQTT decoder**, execute `python3 mqtt_decoder.py` in the `client/src` directory.
 - This prints each data point pulled from TTN to console

To run only the **Flask server**, execute `flask --app flask_app run` in the `client/src` directory.
 - The Flask server can be accessed at [`http://127.0.0.1:5000/`](http://127.0.0.1:5000/)
 - This prints web server status information to console

## How to Run (Server)

Connect the `Heltec WiFi LoRa 32 (V3)` board and `NEO-6M` GPS module
 - Pin `26` on the Heltec board goes to `Rx` on the `NEO-6M`
 - Pin `47` on the Heltec board goes to `Tx` on the `NEO-6M`

Flash the code in the `server/src` to the Heltec board. 
 - Once booted up, it may take several minutes for the `NEO-6M` to receive data while it handles GPS triangulation.
 - Make sure you are in range of a configured [TTN LoRa gateway](https://www.thethingsnetwork.org/docs/gateways/).


## Site UI

<div align="center">
    <img src="https://github.com/ncooney/transit-tracker/blob/main/client/static/site.png" alt="Site UI">
</div
