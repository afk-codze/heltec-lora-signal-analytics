#include <Arduino.h>
#include <arduinoFFT.h>

const uint16_t SAMPLES = 64;
const double SIGNAL_FREQUENCY = 200.0;  // Hz
const double SAMPLING_FREQUENCY = 5000.0; // Hz
const double AMPLITUDE = 100.0; 

// FFT input/output arrays
double vReal[SAMPLES];
double vImag[SAMPLES];

ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);

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
    vReal[i] = AMPLITUDE * sin(i * ratio) / 2.0;
    vImag[i] = 0.0;
  }

  FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, SAMPLES);

  double peakFrequency = FFT.majorPeak();

  Serial.print("Dominant Frequency: ");
  Serial.print(peakFrequency, 2);
  Serial.println(" Hz");

  delay(2000);
}
