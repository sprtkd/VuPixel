/**
   Vumeter using FFT
   Suitable for signals up to 4.5 KHz
   Sampling frequency on Arduino Uno is approximately 9 KHz
   Uses the FIX_FFT library:    https://www.arduinolibraries.info/libraries/fix_fft
*/

/**
   Created by Suprotik Dey
   Shout out to Gadget Reboot
*/

#include <fix_fft.h> // fixed point Fast Fourier Transform library
#include <FastLED.h>
#include <LEDMatrix.h>
#define AUDIO_IN A0 // audio input port
#define PIXEL_COLS 20 //Not greater than 64
#define PIXEL_ROWS 10
#define MATRIX_SIZE (PIXEL_COLS*PIXEL_ROWS)
#define MATRIX_LAYOUT VERTICAL_ZIGZAG_MATRIX
#define FFT_OUTPUT_BUFFER_SIZE 64
#define READ_BUFFER_SIZE FFT_OUTPUT_BUFFER_SIZE*2
#define FFT_OUT_MAX_AMP 100
#define AUDIO_GAIN 1.2
#define BAR_GAIN 1
#define AUDIO_NOISE_THRESHOLD 2
#define PIXEL_OUTPUT_PIN 12
#define LED_BRIGHTNESS 70
#define HUE_SHIFTER 255/PIXEL_COLS

DEFINE_GRADIENT_PALETTE(BlueToRed)
{
  0, 0, 0, 255,
  255,  255, 0, 0
};

cLEDMatrix < PIXEL_COLS, PIXEL_ROWS, MATRIX_LAYOUT > leds;
CRGBPalette16 hueBlueToRed = BlueToRed;

void setup() {
  FastLED.addLeds<WS2812B, PIXEL_OUTPUT_PIN, GRB>(leds[0], leds.Size());
  FastLED.setBrightness(LED_BRIGHTNESS);
  FastLED.clear(true);
  analogReference(DEFAULT); // use the default analog reference of 5 volts (on 5V Arduino boards)
  pinMode(AUDIO_IN, INPUT);
  Serial.begin(9600); // setup serial
};

void loop() {
  //spectrumAnalyzeFlow();
  vumeterFlow();
};

void spectrumAnalyzeFlow() {
  char realFFTRes[READ_BUFFER_SIZE], imaginaryFFTRes[READ_BUFFER_SIZE]; // real and imaginary FFT result arrays
  int outFreqVals[PIXEL_COLS], fftOutBuf[FFT_OUTPUT_BUFFER_SIZE];
  readAndScale(realFFTRes, imaginaryFFTRes);
  fix_fft(realFFTRes, imaginaryFFTRes, 7, 0); //library call
  convertOutput(realFFTRes, imaginaryFFTRes, fftOutBuf);
  scaleOutput(fftOutBuf, outFreqVals);
  //printData(outFreqVals, PIXEL_COLS);
  lightUpLEDMatrix(outFreqVals);
}

void vumeterFlow() {
  int reading = map(getSample(), 0, 1023, 0, PIXEL_ROWS);
  int outFreqVals[PIXEL_COLS];
  for (int i = 0; i < PIXEL_COLS; i++) {
    outFreqVals[i] = reading;
  }
  lightUpLEDMatrix(outFreqVals);
}

void readAndScale(char re[], char im[]) {
  // The FFT real/imaginary data are stored in a char data type as a signed -128 to 127 number
  // This allows a waveform to swing centered around a 0 reference data point
  // The ADC returns data between 0-1023 so it is scaled to fit within a char by dividing by 4 and subtracting 128.
  // eg (0 / 4) - 128 = -128 and (1023 / 4) - 128 = 127
  for (int i = 0; i < READ_BUFFER_SIZE; i++) {
    re[i] = getSample() / 4 - 128; // scale the samples to fit within a char variable
    im[i] = 0; // there are no imaginary samples associated with the time domain so set to 0
  }
}

int getSample() {
  int sample = analogRead(AUDIO_IN); // read analog input samples from ADC
  if (sample <= AUDIO_NOISE_THRESHOLD)
    sample = 0;
  float sampleAmped = (float) sample * AUDIO_GAIN;
  if (sampleAmped > 1023)
    sampleAmped = 1023;
  return sampleAmped;
}

void convertOutput(char re[], char im[], int out[]) {
  // The data array will contain frequency bin data in locations 0..40 for samples up to the sampling frequency of approx. 9 KHz
  // Each frequency bin will represent a center frequency of approximately (9 KHz / 40 samples) = 225 Hz
  // Due to Nyquist sampling requirements, we can only consider sampled frequency data up to (sampling rate / 2) or (9 KHz / 2) = 4.5 KHz
  // Therefore we only acknowledge the first 64 frequency bins [0..20] = [0..4.5KHz]
  for (int i = 0; i < FFT_OUTPUT_BUFFER_SIZE; i++) {
    out[i] = sqrt(re[i] * re[i] + im[i] * im[i]);     // frequency magnitude is the square root of the sum of the squares of the real and imaginary parts of a vector
  };
}

void scaleOutput(int fftOut[], int scaledOut[]) {
  int i, j;
  //scaling by columns
  int colScaler = FFT_OUTPUT_BUFFER_SIZE / PIXEL_COLS;
  if (colScaler * PIXEL_COLS < FFT_OUTPUT_BUFFER_SIZE) {
    colScaler = colScaler + 1;
  }
  for (i = 0; i < PIXEL_COLS; i++) {
    int currAvgOut = 0;
    for (j = 0; j < colScaler; j++) {
      int currFFtPos = (i * colScaler) + j;
      if (currFFtPos >= FFT_OUTPUT_BUFFER_SIZE) {
        break;
      }
      currAvgOut = currAvgOut + fftOut[currFFtPos];
    }
    scaledOut[i] = (float)currAvgOut / j;
  }

  //scaling by rows
  for (i = 0; i < PIXEL_COLS; i++) {
    scaledOut[i] = (float) scaledOut[i] * BAR_GAIN;
    if (scaledOut[i] > FFT_OUT_MAX_AMP)
      scaledOut[i] = FFT_OUT_MAX_AMP;
    scaledOut[i] = map(scaledOut[i], 0, FFT_OUT_MAX_AMP, 0, PIXEL_ROWS);
  }
}

void printData(int outData[], int sizeofData) {
  for (int i = 0; i < sizeofData; i++) {
    Serial.print(outData[i]); // debug value
    Serial.print(","); // debug value
  }
  Serial.println();
}

void lightUpLEDMatrix(int ledData[]) {
  FastLED.clear();
  for (int i = 0; i < PIXEL_COLS; i++)
  {
    leds.DrawLine(i, 0, i, ledData[i], ColorFromPalette(hueBlueToRed, (int)(HUE_SHIFTER * i)));
  }
  FastLED.show();
}
