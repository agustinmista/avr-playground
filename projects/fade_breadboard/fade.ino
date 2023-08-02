#define LED 9

int brightness = 0;
int step = 5;

void setup()  {
  pinMode(LED, OUTPUT);
}

void loop()  {
  analogWrite(LED, brightness);

  brightness = brightness + step;

  if (brightness == 0 || brightness == 255) {
    step = -step;
  }

  delay(10);
}