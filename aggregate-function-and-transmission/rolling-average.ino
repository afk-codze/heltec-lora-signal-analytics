#include <Arduino.h>
#include <FreeRTOS.h>

// -----------------------------------------------------------------------------
// Parameters
// -----------------------------------------------------------------------------

static const double GENERATOR_RATE     = 5000.0;  // "simulation rate" for generator task (Hz)

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

// -----------------------------------------------------------------------------
// Task A: Generate a composite signal at ~5000 steps/sec
// -----------------------------------------------------------------------------
void generateSignalTask(void *pvParameters)
{
  const double dt = 1.0 / GENERATOR_RATE; // Time step in seconds
  double t = 0.0;

  for (;;) // infinite loop
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
// Task B: Sample continuously at ~410 Hz and compute a 2-second sliding average
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
      ringIndex = 0; // wrap around
    }

    // 5) Compute the rolling average
    double rollingAverage = ringSum / AVERAGE_WINDOW_SAMPLES;

    Serial.print(newSample);
    Serial.print(" ");
    Serial.println(rollingAverage);

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

  // Task A: generates the composite sine wave
  xTaskCreate(
    generateSignalTask, // Pointer to the function implementing the task
    "GenerateTask",     // Task name (for debugging)
    2048,               // Stack
    NULL,               // Task parameters (not used)
    1,                  // Task priority (1 is low, 5 is high)
    NULL                // Task handle (not used)
  );

  // Task B: samples the wave and computes  rolling average
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

}
