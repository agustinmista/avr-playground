#define LED 9
#define LDR A5

int brightness = 0;
int step = 5;

void setup() {
  // Set pin modes
  pinMode(LED, OUTPUT);
}

void loop() {
  brightness = map(analogRead(LDR), 0, 100, 255, 0);
  analogWrite(LED, brightness);
}