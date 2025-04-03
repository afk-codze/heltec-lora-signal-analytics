#include <Arduino.h>
#include <FreeRTOS.h>
#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include "secrets.h"

// -----------------------------------------------------------------------------
// Parameters
// -----------------------------------------------------------------------------

// Signal generation parameters
static const double GENERATOR_RATE     = 5000.0;  // "simulation rate" for generator task

static const double FREQ1              = 150.0;   // Hz
static const double AMP1               = 2.0;

static const double FREQ2              = 200.0;   // Hz
static const double AMP2               = 4.0;

static const double SAMPLER_FREQUENCY  = 410.0;   // Hz 
static const int    SAMPLER_PERIOD_MS  = (int)(1000.0 / SAMPLER_FREQUENCY + 0.5); // convert to ms from SAMPLER_FREQUENCY

static const double AVERAGE_WINDOW_SEC = 0.1; //sliding window size in seconds
static const int    AVERAGE_WINDOW_SAMPLES = (int)(SAMPLER_FREQUENCY * AVERAGE_WINDOW_SEC + 0.5); // # of samples in the averaging window

// -----------------------------------------------------------------------------
// Global Shared Data
// -----------------------------------------------------------------------------

// This is updated by Task A and read by Task B.
volatile double g_currentSignalValue = 0.0;

// Create Adafruit IO instance and feed globally
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
AdafruitIO_Feed *avgFeed = io.feed("sensor-rolling-average");

// -----------------------------------------------------------------------------
// Task A: Generate a composite signal at ~5000 steps/sec
// -----------------------------------------------------------------------------
void generateSignalTask(void *pvParameters)
{
  const double dt = 1.0 / GENERATOR_RATE; // Time step in seconds
  double t = 0.0;

  for (;;)
  {
    // Composite signal: 2*sin(2π*150*t) + 4*sin(2π*200*t)
    double sample = AMP1 * sin(2.0 * PI * FREQ1 * t) +
                    AMP2 * sin(2.0 * PI * FREQ2 * t);

    g_currentSignalValue = sample;

    // Advance time
    t += dt;
    if (t >= 1.0) {
      t -= 1.0; // wrap around after 1 second
    }

    vTaskDelay(pdMS_TO_TICKS(1)); // delay 1 ms converted to ticks
  }
}

// -----------------------------------------------------------------------------
// Task B: Sample continuously at ~410 Hz and compute a 0.1-second rolling average
// -----------------------------------------------------------------------------
void sampleSignalTask(void *pvParameters)
{
  // Ring buffer to hold the last ~41 samples (0.1 seconds worth at 410 Hz)
  static double ringBuffer[AVERAGE_WINDOW_SAMPLES];

  double ringSum = 0.0;
  int ringIndex = 0;

  // Initialize the buffer to zeros
  for (int i = 0; i < AVERAGE_WINDOW_SAMPLES; i++)
  {
    ringBuffer[i] = 0.0;
  }

  for (;;)
  {
    // Sliding window implementation
    // 1) Read the latest sample from Task A
    double newSample = g_currentSignalValue;

    // 2) Remove the oldest sample from the sum
    ringSum -= ringBuffer[ringIndex];

    // 3) Place the new sample in the buffer and add it to the sum
    ringBuffer[ringIndex] = newSample;
    ringSum += newSample;

    // 4) Advance the ring buffer index (circular buffer)
    ringIndex++;
    if (ringIndex >= AVERAGE_WINDOW_SAMPLES) {
      ringIndex = 0;
    }

    // 5) Compute the rolling average
    double rollingAverage = ringSum / AVERAGE_WINDOW_SAMPLES;

    // 6) Print the instantaneous sample and the rolling average
    Serial.print(newSample);
    Serial.print(" ");
    Serial.println(rollingAverage);

    // 7) Publish the rolling average to Adafruit IO feed
    char payload[20];
    snprintf(payload, sizeof(payload), "%.2f", rollingAverage);
    avgFeed->save(payload);

    vTaskDelay(pdMS_TO_TICKS(SAMPLER_PERIOD_MS));
  }
}

// -----------------------------------------------------------------------------
// Arduino Setup
// -----------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  while (!Serial) { /* wait for Serial Monitor */ }
  Serial.println("Starting RTOS tasks...");

  // Connect to Adafruit IO
  io.connect();

  // Wait until connected to Adafruit IO
  while (io.status() < AIO_CONNECTED) {
    Serial.println(io.statusText());
    delay(500);
  }
  Serial.println("Connected to Adafruit IO!");

  // Create Task A: generates the composite sine wave
  xTaskCreate(
    generateSignalTask, // Pointer to the function implementing the task
    "GenerateTask",     // Task name (for debugging)
    2048,               // Stack
    NULL,               // Task parameters (not used)
    1,                  // Task priority (1 is low, 5 is high)
    NULL                // Task handle (not used)
  );

  // Create Task B: samples the signal and computes a 0.1-second rolling average
  xTaskCreate(
    sampleSignalTask,
    "SampleTask",
    4096,
    NULL,
    1,
    NULL
  );
}

// -----------------------------------------------------------------------------
// Arduino Loop
// -----------------------------------------------------------------------------
void loop()
{
  // Process Adafruit IO tasks
  io.run();
}
