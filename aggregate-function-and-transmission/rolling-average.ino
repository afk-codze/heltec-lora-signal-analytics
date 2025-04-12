#include <Arduino.h>
#include <FreeRTOS.h>
#include <queue.h>

// -----------------------------------------------------------------------------
// Parameters
// -----------------------------------------------------------------------------

static const double GENERATOR_RATE     = 1000.0;  // "simulation rate" for generator task (Hz)

static const double FREQ1              = 150.0;   // Hz
static const double AMP1               = 2.0;

static const double FREQ2              = 200.0;   // Hz
static const double AMP2               = 4.0;

static const double SAMPLER_FREQUENCY  = 410.0;   // Hz
static const int    SAMPLER_PERIOD_MS  = (int)(1000.0 / SAMPLER_FREQUENCY + 0.5); // convert to ms from SAMPLER_FREQUENCY

static const double AVERAGE_WINDOW_SEC = 0.1; // sliding window size in seconds
static const int    AVERAGE_WINDOW_SAMPLES = (int)(SAMPLER_FREQUENCY * AVERAGE_WINDOW_SEC + 0.5); // # of samples in the averaging window

// -----------------------------------------------------------------------------
// Global Shared Data
// -----------------------------------------------------------------------------

QueueHandle_t signalQueue; // now used instead of g_currentSignalValue

// -----------------------------------------------------------------------------
// Task A: Generate a composite signal at ~1000 steps/sec
// -----------------------------------------------------------------------------
void generateSignalTask(void *pvParameters)
{
  const double dt = 1.0 / GENERATOR_RATE; // Time step in seconds
  double t = 0.0;

  for (;;) // infinite loop
  {
    double sample = AMP1 * sin(2.0 * PI * FREQ1 * t) +
                    AMP2 * sin(2.0 * PI * FREQ2 * t);

    xQueueOverwrite(signalQueue, &sample); // replace previous sample if full

    // Advance time
    t += dt;
    if (t >= 1.0) {
      t -= 1.0; // wrap around after 1 second
    }

    vTaskDelay(pdMS_TO_TICKS(1000.0 / GENERATOR_RATE)); // delay 1 ms converted to ticks
  }
}

// -----------------------------------------------------------------------------
// Task B: Sample continuously at ~410 Hz and compute a 0.1s sliding average
// -----------------------------------------------------------------------------
void sampleSignalTask(void *pvParameters)
{
  // Ring buffer to hold the sliding window samples
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

    if (xQueueReceive(signalQueue, &newSample, 0) == pdPASS)
    {
      ringSum -= ringBuffer[ringIndex];
      ringBuffer[ringIndex] = newSample;
      ringSum += newSample;

      ringIndex++;
      if (ringIndex >= AVERAGE_WINDOW_SAMPLES) {
        ringIndex = 0; // wrap around
      }

      double rollingAverage = ringSum / AVERAGE_WINDOW_SAMPLES;

      Serial.print(newSample);
      Serial.print(" ");
      Serial.println(rollingAverage);
    }

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

  signalQueue = xQueueCreate(1, sizeof(double));
  if (signalQueue == NULL) {
    Serial.println("Queue creation failed!");
    while (1);
  }

  xTaskCreate(generateSignalTask, "GenerateTask", 2048, NULL, 1, NULL);
  xTaskCreate(sampleSignalTask,   "SampleTask",   4096, NULL, 1, NULL);
}

// -----------------------------------------------------------------------------
// Arduino Loop
// -----------------------------------------------------------------------------
void loop()
{
  // Nothing here
}
