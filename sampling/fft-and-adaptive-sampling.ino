#include <Arduino.h>
#include <arduinoFFT.h>

const uint16_t SAMPLES = 64;                       // Must be a power of 2
const double SAMPLING_FREQUENCY = 5000.0;          // Hz

// Frequencies and amplitudes of the components
const double FREQ1 = 150.0;   // Hz
const double AMP1  = 2.0;

const double FREQ2 = 200.0;   // Hz
const double AMP2  = 4.0;

// FFT input/output arrays
double vReal[SAMPLES];
double vImag[SAMPLES];

ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY); // FFT object

void setup()
{
  Serial.begin(115200);
  while (!Serial); // Wait for Serial Monitor
  Serial.println("Ready");
}

void loop()
{
  // Step 1: Generate the synthetic signal
  for (uint16_t i = 0; i < SAMPLES; i++)
  {
    double t = (double)i / SAMPLING_FREQUENCY; // time in seconds
    vReal[i] = AMP1 * sin(2.0 * PI * FREQ1 * t) +
               AMP2 * sin(2.0 * PI * FREQ2 * t);
    vImag[i] = 0.0;
  }

  // Step 2: Apply windowing and compute FFT
  FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, SAMPLES);

  // Step 3: Find dominant frequency
  double maxMag = 0.0;
  uint16_t peakIndex = 0;

  for (uint16_t i = 1; i < SAMPLES / 2; i++) { // Ignore bin 0 (DC component)
    if (vReal[i] > maxMag) {
      maxMag = vReal[i];
      peakIndex = i;
    }
  }

  double peakFrequency = (peakIndex * SAMPLING_FREQUENCY) / SAMPLES;

  Serial.print("Dominant Frequency: ");
  Serial.print(peakFrequency, 2);
  Serial.println(" Hz");

  Serial.println("----------------------------");
  delay(2000); // Wait before next update
}
