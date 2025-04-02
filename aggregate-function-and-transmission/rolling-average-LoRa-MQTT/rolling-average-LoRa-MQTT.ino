#include <Arduino.h>
#include <FreeRTOS.h>
#include "LoRaWan_APP.h"
#include "secrets.h"

// -----------------------------------------------------------------------------
// LoRaWAN parameters
// -----------------------------------------------------------------------------

/* Channels mask */
uint16_t userChannelsMask[6] = {0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
DeviceClass_t loraWanClass    = CLASS_A;
uint32_t appTxDutyCycle       = 15000;
bool overTheAirActivation     = true;
bool loraWanAdr               = true;
bool isTxConfirmed            = true; // We are using confirmed messages so we want to receive an ACK
uint8_t appPort               = 2;
uint8_t confirmedNbTrials     = 4; // number of trials for confirmed messages (if not received acknowledgment resend at most this number of times)

// -----------------------------------------------------------------------------
// Rolling average / signal generation parameters
// -----------------------------------------------------------------------------
static const double SIGNAL_FREQUENCY = 200.0;  // Hz
static const double GENERATOR_RATE = 5000.0;   // "simulation rate" for generator task
static const double AMPLITUDE = 100.0;

static const double SAMPLER_FREQUENCY = 410.0;  // Hz
static const int SAMPLER_PERIOD_MS = (int)(1000.0 / SAMPLER_FREQUENCY + 0.5);

static const double AVERAGE_WINDOW_SEC = 0.1;
static const int AVERAGE_WINDOW_SAMPLES =
  (int)(SAMPLER_FREQUENCY * AVERAGE_WINDOW_SEC + 0.5);

// Shared data between tasks
volatile double g_currentSignalValue = 0.0;
volatile double g_latestRollingAverage = 0.0;

// -----------------------------------------------------------------------------
// Task A: Generate a 200 Hz sine wave at ~5000 steps/sec
// -----------------------------------------------------------------------------
void generateSignalTask(void *pvParameters) {
  double stepAngle = 2.0 * PI * SIGNAL_FREQUENCY / GENERATOR_RATE;
  double angle = 0.0;

  for (;;) {
    double sample = (AMPLITUDE * sin(angle)) / 2.0;
    g_currentSignalValue = sample;

    // Advance angle
    angle += stepAngle;
    if (angle >= 2.0 * PI) {
      angle -= 2.0 * PI;
    }

    // ~1 ms delay => ~5000 samples/sec
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// -----------------------------------------------------------------------------
// Task B: Sample at ~410 Hz + compute 0.1-second rolling average
// -----------------------------------------------------------------------------
void sampleSignalTask(void *pvParameters) {
  static double ringBuffer[AVERAGE_WINDOW_SAMPLES];
  double ringSum = 0.0;
  int ringIndex = 0;

  // Initialize
  for (int i = 0; i < AVERAGE_WINDOW_SAMPLES; i++) {
    ringBuffer[i] = 0.0;
  }

  for (;;) {
    double newSample = g_currentSignalValue;

    // Remove oldest
    ringSum -= ringBuffer[ringIndex];
    // Insert new
    ringBuffer[ringIndex] = newSample;
    ringSum += newSample;

    // Advance index
    ringIndex++;
    if (ringIndex >= AVERAGE_WINDOW_SAMPLES) {
      ringIndex = 0;
    }

    // Rolling average
    double rollingAverage = ringSum / AVERAGE_WINDOW_SAMPLES;
    g_latestRollingAverage = rollingAverage;

    // ~2.44 ms => ~410 Hz
    vTaskDelay(pdMS_TO_TICKS(SAMPLER_PERIOD_MS));
  }
}

// -----------------------------------------------------------------------------
// Prepare uplink: fill the library's appData[] with our rolling average
// -----------------------------------------------------------------------------
static void prepareTxFrame(uint8_t port) {
  // Send rolling average as a 4-byte float
  float val = (float)g_latestRollingAverage;

  // The Heltec library gives us global appData[] & appDataSize to be set!
  memcpy(appData, &val, sizeof(float));
  appDataSize = sizeof(float);
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) { /* wait for monitor */
  }

  Serial.println("Starting Heltec + RTOS tasks...");

  // Init Heltec hardware & LoRa
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  // Set initial device state (uses Heltec's global deviceState)
  deviceState = DEVICE_STATE_INIT;

  // Create Task A: generate sine wave
  xTaskCreate(
    generateSignalTask,
    "GenerateTask",
    2048,
    NULL,
    1,
    NULL);

  // Create Task B: sample + rolling avg
  xTaskCreate(
    sampleSignalTask,
    "SampleTask",
    4096,
    NULL,
    1,
    NULL);
}

// -----------------------------------------------------------------------------
// Arduino Loop: Heltec LoRaWAN state machine
// -----------------------------------------------------------------------------
void loop() {
  switch (deviceState) {
    case DEVICE_STATE_INIT:
      {
        #if (LORAWAN_DEVEUI_AUTO)
          LoRaWAN.generateDeveuiByChipID();
        #endif
        LoRaWAN.init(loraWanClass, loraWanRegion);

        // Default data rate DR3 for EU868
        LoRaWAN.setDefaultDR(3);

        deviceState = DEVICE_STATE_JOIN;
        break;
      }

    case DEVICE_STATE_JOIN:
      {
        // OTAA in our case -> overTheAirActivation = true;
        LoRaWAN.join();
        break;
      }

    case DEVICE_STATE_SEND:
      {
        prepareTxFrame(appPort);
        LoRaWAN.send();

        deviceState = DEVICE_STATE_CYCLE;
        break;
      }

    case DEVICE_STATE_CYCLE:
      {
        // Next uplink in appTxDutyCycle plus random offset
        txDutyCycleTime =
          appTxDutyCycle + randr(-APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND); //defaulted by Heltec library to 1000ms

        LoRaWAN.cycle(txDutyCycleTime);
        deviceState = DEVICE_STATE_SLEEP;
        break;
      }

    case DEVICE_STATE_SLEEP:
      {
        // Sleep radio until next cycle
        LoRaWAN.sleep(loraWanClass);
        break;
      }

    default:
      {
        deviceState = DEVICE_STATE_INIT; // Reset to init state in case of error or disconnection
        break;
      }
  }
}
