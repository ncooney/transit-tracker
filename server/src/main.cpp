#include "LoRaWan_APP.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <TinyGPS++.h>
#include <esp_timer.h>
#include "driver/temp_sensor.h"
#include <stdio.h>

// General macros
#define LORA_GEN_PERIOD_MS(x)       (x > 2000 ? (uint64_t)(x - 2000) : (uint64_t)(x))
#define LORA_PERIOD_MS              (LORA_GEN_PERIOD_MS(20000)) // 20 s
#define TIMER_PERIOD_US             (10000)                     // 10 ms
#define BAUDE_RATE                  (115200)
#define GPS_BAUDE_RATE              (9600)
#define TX_PIN                      (26)
#define RX_PIN                      (47)

// Communication protocol macros
#define DEVICE_ID_ASCII             (49)                        // '1'
#define TYPE_DEV_ID                 (0x00)
#define TYPE_LATITUDE               (0x01)
#define TYPE_LONGITUDE              (0x02)

// General GPS config
HardwareSerial GpioSerial(2);
TinyGPSPlus gps;
bool gps_updated = false;
double geo_lat =  69.420420;    // Default for debug
double geo_lon = -69.420420;    // Default for debug

// General LoRa config
uint16_t userChannelsMask[6]  = {0xFF00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
DeviceClass_t loraWanClass    = CLASS_A;
bool loraWanAdr               = true;
bool isTxConfirmed            = true;
uint8_t appPort               = 1;
uint32_t appTxDutyCycle       = LORA_PERIOD_MS;
uint8_t confirmedNbTrials     = 8;
RTC_DATA_ATTR bool first_run  = true;

// LoRa OTAA config
bool overTheAirActivation = true;
uint8_t appEui[] = {/* TODO */};
uint8_t devEui[] = {/* TODO */};
uint8_t appKey[] = {/* TODO */};

// LoRa ABP config (not used here)
uint32_t devAddr  = (uint32_t)0x00000000;
uint8_t nwkSKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t appSKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/**
 * @brief Timer ISR, used to periodically check for GPS data
*/
static void IRAM_ATTR timer_ISR(void *arg)
{
    // Check UART is available
    if (GpioSerial.available() > 0)
    {
        // Check data updated
        gps.encode(GpioSerial.read());
        if (gps.location.isUpdated())
        {
            // Record the updated values
            Serial.println("GPS updated");
            gps_updated = true;
            geo_lat = gps.location.lat();
            geo_lon = gps.location.lng();
        }
    }
}

/**
 * @brief Helper function to build a device ID message
 * 
 * @param data The buffer to place the message in
 * @param start Where to start filling the buffer
 * @return The number of characters added to the buffer
*/
static uint8_t build_dev_id_msg(uint8_t* data, uint8_t start)
{
    // Prepare the message length
    uint8_t tlv_length = 1;
    uint8_t msg_length = tlv_length + 2;

    // Fill the buffer
    data[start]     = tlv_length;
    data[start + 1] = TYPE_DEV_ID;
    data[start + 2] = DEVICE_ID_ASCII;

    return msg_length;
}

/**
 * @brief Helper function to build a latitude or longitude message
 * 
 * @param data The buffer to place the message in
 * @param start Where to start filling the buffer
 * @return The number of characters added to the buffer
*/
static uint8_t build_geo_msg(uint8_t* data, uint8_t start, uint8_t type, double geo)
{
    // Prepare the message length
    uint8_t tlv_length = 11;
    uint8_t msg_length = tlv_length + 2;

    // Fill the buffer
    data[start]     = tlv_length;
    data[start + 1] = type;
    sprintf((char *) &data[start + 2], "%11.6f", geo);

    return msg_length;
}

/**
 * @brief Prepares a payload for the transmission frame
 *
 * Payload is one or more (length, type, value) tuples. Format:
 *  |---------------------------|
 *  | Length            [1 byte]|
 *  |---------------------------|
 *  | Type              [1 byte]|
 *  |---------------------------|
 *  | Value     [variable bytes]|
 *  |---------------------------|
 *  | ...                       |
 *  |---------------------------|
 *
 * Notes:
 *  - Length includes only the value field, not length, or type fields
 *  - Type = 0x00 indicates device ID
 *  - Type = 0x01 indicates latitude reading
 *  - Type = 0x02 indicates longitude reading
 */
static void prepare_tx_frame(uint8_t port)
{
    appDataSize = 0;
    if (gps_updated)
    {
        // Build device_id message
        appDataSize = 0;
        appDataSize += build_dev_id_msg(appData, appDataSize);
        appDataSize += build_geo_msg(appData, appDataSize, TYPE_LATITUDE, geo_lat);
        appDataSize += build_geo_msg(appData, appDataSize, TYPE_LONGITUDE, geo_lon);
        gps_updated = false;

        Serial.printf("transmitting: [");
        for (uint8_t i = 0; i < appDataSize; i++)
        {
            Serial.printf("%d,", appData[i]);
        }
        Serial.printf("]\n");
    }
}

/**
 * @brief Initializes the temperature sensor and LoRa communication
 */
void setup()
{
    // Configure serial baude rate
    Serial.begin(BAUDE_RATE);
    GpioSerial.begin(GPS_BAUDE_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

    // Configure timer
    esp_timer_handle_t timer;
    esp_timer_create_args_t timer_args = {
        .callback = &timer_ISR,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "GPS Timer",
    };
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_periodic(timer, TIMER_PERIOD_US);

    // Configure LoRaWAN
    Mcu.begin();
    if (first_run)
    {
        LoRaWAN.displayMcuInit();
        first_run = false;
    }
    deviceState = DEVICE_STATE_INIT;
}

/**
 * @brief Orchestrate LoRaWAN finite state machine
 */
void loop()
{
    // FSM based on deviceState
    switch (deviceState)
    {
        case DEVICE_STATE_INIT:
            LoRaWAN.init(loraWanClass, loraWanRegion);
        break;

        case DEVICE_STATE_JOIN:
            LoRaWAN.displayJoining();
            LoRaWAN.join();
            if (deviceState == DEVICE_STATE_SEND)
            {
                LoRaWAN.displayJoined();
            }
        break;

        case DEVICE_STATE_SEND:
            LoRaWAN.displaySending();
            prepare_tx_frame(appPort);
            LoRaWAN.send();
            deviceState = DEVICE_STATE_CYCLE;
        break;

        case DEVICE_STATE_CYCLE:
            // Schedule next packet transmission within 1s of appTxDutyCycle
            txDutyCycleTime = appTxDutyCycle + randr(0, APP_TX_DUTYCYCLE_RND);
            LoRaWAN.cycle(txDutyCycleTime);
            deviceState = DEVICE_STATE_SLEEP;
        break;

        case DEVICE_STATE_SLEEP:
            LoRaWAN.displayAck();
            LoRaWAN.sleep(loraWanClass); // Note: system resets to wake
        break;

        default:
            deviceState = DEVICE_STATE_INIT;
        break;
    }
}
