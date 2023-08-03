#define LED 9
#define LDR A5

int brightness = 0;
int step = 5;

void setup()  {
  pinMode(LED, OUTPUT);
}

void loop()  {
  brightness = map(analogRead(LDR), 0, 500, 255, 0);
  analogWrite(LED, brightness);
}