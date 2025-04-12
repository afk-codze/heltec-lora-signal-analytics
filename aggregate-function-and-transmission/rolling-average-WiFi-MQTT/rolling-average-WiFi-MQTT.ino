#include <Arduino.h>
#include <FreeRTOS.h>
#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include "secrets.h"

// -----------------------------------------------------------------------------
// Parameters
// -----------------------------------------------------------------------------

// Signal generation parameters
static const double GENERATOR_RATE     = 1000.0;  // "simulation rate" for generator task (Hz)

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

// Queue used to pass the signal sample from generator to sampler tasks
QueueHandle_t signalQueue;

// Create Adafruit IO instance and feed globally
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
AdafruitIO_Feed *avgFeed = io.feed("sensor-rolling-average");

// -----------------------------------------------------------------------------
// Task A: Generate a composite signal at ~1000 samples/sec
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

    // Pass the sample to the sampler task via queue (overwrite mode)
    xQueueOverwrite(signalQueue, &sample);

    // Advance time
    t += dt;
    if (t >= 1.0) {
      t -= 1.0; // wrap around after 1 second
    }

    // Delay to match GENERATOR_RATE (5000 Hz)
    vTaskDelay(pdMS_TO_TICKS(1000.0 / GENERATOR_RATE));
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
    double newSample = 0.0;

    // Read the latest signal sample from the queue
    if (xQueueReceive(signalQueue, &newSample, 0) == pdPASS)
    {
      // Sliding window implementation
      // 1) Remove the oldest sample from the sum
      ringSum -= ringBuffer[ringIndex];

      // 2) Place the new sample in the buffer and add it to the sum
      ringBuffer[ringIndex] = newSample;
      ringSum += newSample;

      // 3) Advance the ring buffer index (circular buffer)
      ringIndex++;
      if (ringIndex >= AVERAGE_WINDOW_SAMPLES) {
        ringIndex = 0;
      }

      // 4) Compute the rolling average
      double rollingAverage = ringSum / AVERAGE_WINDOW_SAMPLES;

      // 5) Print the instantaneous sample and the rolling average
      Serial.print(newSample);
      Serial.print(" ");
      Serial.println(rollingAverage);

      // 6) Publish the rolling average to Adafruit IO feed
      char payload[20];
      snprintf(payload, sizeof(payload), "%.2f", rollingAverage);
      avgFeed->save(payload);
    }

    // Delay to match SAMPLER_FREQUENCY (410 Hz)
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

  // Create a queue to hold 1 signal sample (double)
  signalQueue = xQueueCreate(1, sizeof(double));
  if (signalQueue == NULL) {
    Serial.println("Queue creation failed!");
    while (1);
  }

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
