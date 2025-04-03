#include <Arduino.h>
#include <arduinoFFT.h>

const uint16_t SAMPLES = 64;
const double SIGNAL_FREQUENCY = 200.0;  // Hz
const double SAMPLING_FREQUENCY = 5000.0; // Hz
const double AMPLITUDE = 100.0; 

// FFT input/output arrays
double vReal[SAMPLES];
double vImag[SAMPLES];

ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY); //FFT object

void setup()
{
  Serial.begin(115200);
  while(!Serial) { /* Wait for Serial Monitor to connect */ }
  Serial.println("Ready");
}

void loop()
{
  double ratio = (2.0 * PI * SIGNAL_FREQUENCY / SAMPLING_FREQUENCY);
  for (uint16_t i = 0; i < SAMPLES; i++)
  {
    // Arrays to hold the real and imaginary parts of the signal
    vReal[i] = AMPLITUDE * sin(i * ratio) / 2.0;
    vImag[i] = 0.0;
  }

  FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD); //prepare the data (reduce spectral leakage)
  FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD); //compute FFT
  FFT.complexToMagnitude(vReal, vImag, SAMPLES);   // Convert the complex output of FFT to magnitude

  double peakFrequency = FFT.majorPeak(); // Find the peak frequency

  Serial.print("Dominant Frequency: ");
  Serial.print(peakFrequency, 2);
  Serial.println(" Hz");

  delay(2000);
}
