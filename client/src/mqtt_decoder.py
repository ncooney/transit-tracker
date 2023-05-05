import paho.mqtt.client as mqtt
import base64
import configparser
import json
import struct
import os.path
from datetime import datetime
from geopy.geocoders import GoogleV3

# Global defines
DATA_FILE_NAME = os.path.dirname(__file__) + "/../data/gps_data_live.txt"
CONF_FILE_NAME = os.path.dirname(__file__) + "/../config.ini"
TYPE_DEVICE_ID = 0
TYPE_LAT_READ  = 1
TYPE_LON_READ  = 2
GEOLOCATOR     = None

# Main method
def main():
    
    # Load configuration
    config = configparser.ConfigParser()
    config.read(CONF_FILE_NAME)
    BROKER_ADDR = config["mqtt"]["broker"]
    PORT        = int(config["mqtt"]["port"])
    USERNAME    = config["mqtt"]["username"]
    PASSWORD    = config["mqtt"]["password"]
    API_KEY     = config["mqtt"]["api_key"]

    # Configure MQTT client
    client  = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    # Connect MQTT client to broker (TTN)
    client.username_pw_set(USERNAME, PASSWORD)
    client.tls_set()
    client.connect(BROKER_ADDR, PORT)

    # Subscribe to uplink messages
    topic = "v3/+/devices/+/up"
    client.subscribe(topic)

    # Begin MQTT client loop
    client.loop_forever()

# Helper function to retrieve the API KEY from the config file
def get_api_key():
    config = configparser.ConfigParser()
    config.read(CONF_FILE_NAME)
    return config["mqtt"]["api_key"]

# Callback for MQTT client connected to broker
def on_connect(client, userdata, flags, rc):
    
    # Print the status
    print("Client connected!")
    if (rc != 0):
        print("MQTT connection failed, error {}!".format(rc))


# Callback for incoming MQTT message
def on_message(client, userdata, message):

    # Record the receive timestamp
    timestamp = datetime.now().strftime("%B %d, %H:%M")

    # Attempt to decode the payload
    try:
        # Retrieve the payload from the MQTT message
        message_str = message.payload.decode("utf-8")
        message_json = json.loads(message_str)
        encoded_payload = message_json["uplink_message"]["frm_payload"]
        raw_payload = base64.b64decode(encoded_payload)

        # Make sure the payload is non-empty
        if (len(raw_payload) == 0):
            return

        decoded_payload = decode(raw_payload)
        if (decoded_payload['device_id'] == 0):
            return
    except:
        return
    
    # If decoding was successful, produce output
    GEOLOCATOR = GoogleV3(api_key=get_api_key())
    output = "Device {} arrived at {} ({}, {}) at time {}.\n".format(
        decoded_payload['device_id'],
        str(GEOLOCATOR.geocode("{}, {}".format(
            decoded_payload['latitude'],
            decoded_payload['longitude'],
        ))).replace(" ", "_"),
        decoded_payload['latitude'],
        decoded_payload['longitude'],
        timestamp,
    )
    print(output, end="")
    with open(DATA_FILE_NAME, 'a') as data_file:
        data_file.write(output)

## 
# Payload is one or more (length, type, value) tuples. Format:
#   |--------------------------|
#   | Length [1 byte]          |
#   |--------------------------|
#   | Type   [1 byte]          |
#   |--------------------------|
#   | Value  [variable bytes]  |
#   |--------------------------|
#   | ...                      |
#   |--------------------------|
# 
# Notes:
#  - Length includes only the value field, not length, or type fields
#  - Type = 0x00 indicates device ID
#  - Type = 0x01 indicates latitude reading
#  - Type = 0x02 indicates longitude reading
##
def decode(payload):

    # Dictionary to hold the decoded payload
    decoded_payload = {
        'device_id': 0,
        'latitude':  0,
        'longitude': 0,
    }

    # Loop through all (length, type, value) tuples in packet
    i = 0
    while (i < len(payload)): 

        # Parse this (length, type, value) tuple and update the index
        msg_length = payload[i+0]
        msg_type   = payload[i+1]
        msg_value  = payload[i+2 : (i+2)+msg_length]
        i += (msg_length+2)

        # Handle device IDs
        if (msg_type == TYPE_DEVICE_ID):
            decoded_payload['device_id'] = str(msg_value, "utf-8")

        # Handle latitude readings
        if (msg_type == TYPE_LAT_READ):
            decoded_payload['latitude'] = float(msg_value)
        
        # Handle longitude readings
        if (msg_type == TYPE_LON_READ):
            decoded_payload['longitude'] = float(msg_value)
        
    return decoded_payload

# Read in the data, disposing of non-GPS readings
def collect_data():    
    data = []

    # Open the data file
    with open(DATA_FILE_NAME) as data_file:
        for data_line in data_file:
            
            # Tokenize this data point, convert (latitude,longitude) to address
            data_point = data_line.rstrip().split()
            if (data_point[0] == 'Device'):
                data.append({
                    'device_id': data_point[1],
                    'location':  data_point[4].replace('_', ' '),
                    'latitude':  data_point[5].replace('(', '').replace(',', ''),
                    'longitude': data_point[6].replace(')', ''),
                    'timestamp': " ".join(data_point[9:12]).replace('.', ''),
                })
    
    return data

# Call main
if __name__ == "__main__":
    main()
