
const int vert = 6;
const int rouge = 7;
const int freq = 5000;
const int resolution = 10;
const int pwmChannelVert = 0;
const int pwmChannelRouge = 1;

unsigned long previousMillis = 0;
const long interval = 10; // Intervalle pour mise à jour PWM

int dutyCycle = 0;

enum FadePhase {ETTEINT, FADE_IN, FADE_OUT};
enum ColorMode {VERT, ROUGE, ORANGE};

FadePhase currentPhase = ETTEINT;
ColorMode currentColor = VERT;

unsigned long phaseStartTime = 0;
const unsigned long phaseDuration = 1000; // Fenêtre pour fade in/out en ms
const unsigned long offDuration = 500;   // Délai entre phases éteintes

void setup() {
  ledcAttachChannel(vert, freq, resolution, pwmChannelVert);
  ledcAttachChannel(rouge, freq, resolution, pwmChannelRouge);
  phaseStartTime = millis();
}

void setLEDs(int dutyVert, int dutyRouge) {
  ledcWrite(vert, dutyVert);
  ledcWrite(rouge, dutyRouge);
}

void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - phaseStartTime;

  switch (currentPhase) {
    case ETTEINT:
      setLEDs(0, 0);
      if (elapsed >= offDuration) {
        currentPhase = FADE_IN;
        dutyCycle = 0;
        phaseStartTime = now;
      }
      break;

    case FADE_IN:
      dutyCycle = map(elapsed, 0, phaseDuration, 0, 1023);
      if(dutyCycle > 1023) dutyCycle = 1023;

      if (currentColor == VERT) setLEDs(dutyCycle, 0);
      else if (currentColor == ROUGE) setLEDs(0, dutyCycle);
      else setLEDs(dutyCycle, dutyCycle);

      if (elapsed >= phaseDuration) {
        currentPhase = FADE_OUT;
        phaseStartTime = now;
      }
      break;

    case FADE_OUT:
      dutyCycle = map(elapsed, 0, phaseDuration, 1023, 0);
      if(dutyCycle < 0) dutyCycle = 0;

      if (currentColor == VERT) setLEDs(dutyCycle, 0);
      else if (currentColor == ROUGE) setLEDs(0, dutyCycle);
      else setLEDs(dutyCycle, dutyCycle);

      if (elapsed >= phaseDuration) {
        currentPhase = ETTEINT;
        phaseStartTime = now;
        // Avancer la couleur pour le prochain cycle
        currentColor = static_cast<ColorMode>((static_cast<int>(currentColor) +1) % 3);
      }
      break;
  }
}
