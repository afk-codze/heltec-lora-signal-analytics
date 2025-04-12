#include <Arduino.h>
#include <FreeRTOS.h>
#include "LoRaWan_APP.h"
#include "secrets.h"
#include <arduinoFFT.h>

// -----------------------------------------------------------------------------
// LoRaWAN parameters
// -----------------------------------------------------------------------------

uint16_t userChannelsMask[6] = {0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
DeviceClass_t loraWanClass    = CLASS_A;
uint32_t appTxDutyCycle       = 15000;
bool overTheAirActivation     = true;
bool loraWanAdr               = true;
bool isTxConfirmed            = true;
uint8_t appPort               = 2;
uint8_t confirmedNbTrials     = 4;

// -----------------------------------------------------------------------------
// Rolling average / signal generation parameters
// -----------------------------------------------------------------------------

static const double GENERATOR_RATE = 1000.0;

static const double FREQ1 = 150.0;
static const double AMP1  = 2.0;
static const double FREQ2 = 200.0;
static const double AMP2  = 4.0;

static const double LEARNING_SAMPLING_FREQ = 5000.0;
static const uint16_t FFT_SAMPLES = 64;

static double adaptiveSamplingFreq = 410.0;
static int samplerDelayMs = (int)(1000.0 / adaptiveSamplingFreq + 0.5);

// -----------------------------------------------------------------------------
// Global Shared Queues
// -----------------------------------------------------------------------------

QueueHandle_t signalQueue;
QueueHandle_t averageQueue;

// FFT buffer and object
double vReal[FFT_SAMPLES];
double vImag[FFT_SAMPLES];
ArduinoFFT<double> FFT(vReal, vImag, FFT_SAMPLES, LEARNING_SAMPLING_FREQ);

// -----------------------------------------------------------------------------
// FFT Analysis
// -----------------------------------------------------------------------------

void analyzeFFT() {
  FFT.windowing(vReal, FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(vReal, vImag, FFT_SAMPLES, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, FFT_SAMPLES);

  double maxMag = 0;
  uint16_t peakIndex = 0;

  for (uint16_t i = 1; i < FFT_SAMPLES / 2; i++) {
    if (vReal[i] > maxMag) {
      maxMag = vReal[i];
      peakIndex = i;
    }
  }

  double dominantFreq = (peakIndex * LEARNING_SAMPLING_FREQ) / FFT_SAMPLES;
  adaptiveSamplingFreq = dominantFreq * 2.0;
  samplerDelayMs = (int)(1000.0 / adaptiveSamplingFreq + 0.5);

  Serial.print("FFT complete. Dominant: ");
  Serial.print(dominantFreq, 2);
  Serial.print(" Hz → Adaptive sampling: ");
  Serial.print(adaptiveSamplingFreq, 1);
  Serial.println(" Hz");
}

// -----------------------------------------------------------------------------
// Task A: Generate a composite signal
// -----------------------------------------------------------------------------

void generateSignalTask(void *pvParameters) {
  const double dt = 1.0 / GENERATOR_RATE;
  double t = 0.0;

  for (;;) {
    double sample = AMP1 * sin(2.0 * PI * FREQ1 * t) +
                    AMP2 * sin(2.0 * PI * FREQ2 * t);
    xQueueOverwrite(signalQueue, &sample);
    t += dt;
    if (t >= 1.0) t -= 1.0;
    vTaskDelay(pdMS_TO_TICKS(1000.0 / GENERATOR_RATE));
  }
}

// -----------------------------------------------------------------------------
// Task B: Learn dominant frequency and sample at adaptive rate
// -----------------------------------------------------------------------------

void sampleSignalTask(void *pvParameters) {
  static double ringBuffer[FFT_SAMPLES];
  double ringSum = 0.0;
  int ringIndex = 0;

  for (int i = 0; i < FFT_SAMPLES; i++) ringBuffer[i] = 0.0;

  // Learning phase
  Serial.println("Learning sampling frequency...");
  for (int i = 0; i < FFT_SAMPLES; i++) {
    double sample = 0.0;
    while (xQueueReceive(signalQueue, &sample, portMAX_DELAY) != pdPASS);
    vReal[i] = sample;
    vImag[i] = 0.0;
    vTaskDelay(pdMS_TO_TICKS(1000.0 / LEARNING_SAMPLING_FREQ));
  }

  analyzeFFT();

  for (;;) {
    double newSample = 0.0;
    if (xQueueReceive(signalQueue, &newSample, 0) == pdPASS) {
      ringSum -= ringBuffer[ringIndex];
      ringBuffer[ringIndex] = newSample;
      ringSum += newSample;

      ringIndex++;
      if (ringIndex >= FFT_SAMPLES) ringIndex = 0;

      double rollingAverage = ringSum / FFT_SAMPLES;
      xQueueOverwrite(averageQueue, &rollingAverage);
    }

    vTaskDelay(pdMS_TO_TICKS(samplerDelayMs));
  }
}

// -----------------------------------------------------------------------------
// Prepare uplink
// -----------------------------------------------------------------------------

static void prepareTxFrame(uint8_t port) {
  float val = 0.0f;
  xQueuePeek(averageQueue, &val, 0);
  memcpy(appData, &val, sizeof(float));
  appDataSize = sizeof(float);
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Starting Heltec FFT Adaptive Sampling...");

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  deviceState = DEVICE_STATE_INIT;

  signalQueue = xQueueCreate(1, sizeof(double));
  averageQueue = xQueueCreate(1, sizeof(double));
  if (!signalQueue || !averageQueue) {
    Serial.println("Queue creation failed!");
    while (1);
  }

  xTaskCreate(generateSignalTask, "GenerateTask", 2048, NULL, 1, NULL);
  xTaskCreate(sampleSignalTask, "SampleTask", 4096, NULL, 1, NULL);
}

// -----------------------------------------------------------------------------
// Arduino Loop: LoRaWAN state machine
// -----------------------------------------------------------------------------

void loop() {
  switch (deviceState) {
    case DEVICE_STATE_INIT:
      #if (LORAWAN_DEVEUI_AUTO)
        LoRaWAN.generateDeveuiByChipID();
      #endif
      LoRaWAN.init(loraWanClass, loraWanRegion);
      LoRaWAN.setDefaultDR(3);
      deviceState = DEVICE_STATE_JOIN;
      break;

    case DEVICE_STATE_JOIN:
      LoRaWAN.join();
      break;

    case DEVICE_STATE_SEND:
      prepareTxFrame(appPort);
      LoRaWAN.send();
      deviceState = DEVICE_STATE_CYCLE;
      break;

    case DEVICE_STATE_CYCLE:
      txDutyCycleTime = appTxDutyCycle + randr(-APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND);
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;

    case DEVICE_STATE_SLEEP:
      LoRaWAN.sleep(loraWanClass);
      break;

    default:
      deviceState = DEVICE_STATE_INIT;
      break;
  }
}
