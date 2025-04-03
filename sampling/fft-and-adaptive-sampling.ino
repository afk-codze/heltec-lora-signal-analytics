#include <Arduino.h>
#include <arduinoFFT.h>

const uint16_t SAMPLES = 64;
const double SAMPLING_FREQUENCY = 5000.0; // Hz

// Frequencies and amplitudes of the components
const double FREQ1 = 150;   // Hz
const double AMP1  = 2;

const double FREQ2 = 200;   // Hz
const double AMP2  = 4;

// FFT input/output arrays
double vReal[SAMPLES];
double vImag[SAMPLES];

ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY); // FFT object

void setup()
{
  Serial.begin(115200);
  while (!Serial) { /* Wait for Serial Monitor to connect */ }
  Serial.println("Ready");
}

void loop()
{
  for (uint16_t i = 0; i < SAMPLES; i++)
  {
    double t = (double)i / SAMPLING_FREQUENCY; // time in seconds

    // Composite signal: s(t) = 2*sin(2π*3*t) + 4*sin(2π*5*t)
    vReal[i] = AMP1 * sin(2.0 * PI * FREQ1 * t) +
               AMP2 * sin(2.0 * PI * FREQ2 * t);
    vImag[i] = 0.0;
  }

  FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD); //prepare the data (reduce spectral leakage)
  FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD); //compute FFT
  FFT.complexToMagnitude(vReal, vImag, SAMPLES);   // Convert the complex output of FFT to magnitude

  double peakFrequency = FFT.majorPeak(); // Find dominant frequency

  Serial.print("Dominant Frequency: ");
  Serial.print(peakFrequency, 2);
  Serial.println(" Hz");

  delay(2000);
}
