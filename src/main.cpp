#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_PN532.h>

// PN532 V3: set its mode switches/jumpers to I2C before powering up.
Adafruit_PN532 nfc(0, 0, &Wire); // I2C without separate IRQ/RESET wiring.
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Replace this example with the UID printed by your authorized tag.
// UID bytes are hexadecimal; set AUTHORIZED_UID_LENGTH to 4, 7, or 10.
const uint8_t AUTHORIZED_UID[] = { 0x7F, 0xD5, 0x50, 0xF6 };
const uint8_t AUTHORIZED_UID_LENGTH = sizeof(AUTHORIZED_UID);

bool sameUid(const uint8_t *uid, uint8_t length) {
  if (length != AUTHORIZED_UID_LENGTH) return false;
  for (uint8_t i = 0; i < length; ++i) {
    if (uid[i] != AUTHORIZED_UID[i]) return false;
  }
  return true;
}

void showMessage(const __FlashStringHelper *line1, const __FlashStringHelper *line2 = nullptr) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(line1);
  if (line2) display.println(line2);
  display.display();
}

void animateDoors() {
  // Two panels slide outward; redraw only the small monochrome framebuffer.
  for (int8_t offset = 0; offset <= 48; offset += 4) {
    display.clearDisplay();
    display.drawRect(22, 8, 84, 48, SSD1306_WHITE);
    display.drawLine(64, 8, 64, 55, SSD1306_WHITE);
    display.fillRect(23, 9, 40 - offset / 2, 46, SSD1306_WHITE);
    display.fillRect(65 + offset / 2, 9, 40 - offset / 2, 46, SSD1306_WHITE);
    display.display();
    delay(55);
  }
  showMessage(F("ACESSO LIBERADO"));
  delay(1200);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED nao encontrado (tente endereco 0x3D)"));
    while (true) delay(100);
  }
  showMessage(F("Iniciando PN532..."));
  nfc.begin();
  uint32_t version = nfc.getFirmwareVersion();
  if (!version) {
    Serial.println(F("PN532 nao encontrado. Confira modo I2C e ligacoes."));
    showMessage(F("PN532 nao encontrado"), F("Confira modo I2C"));
    while (true) delay(100);
  }
  nfc.SAMConfig();
  showMessage(F("Aproxime uma tag"));
  Serial.println(F("PN532 pronto. Aproxime uma tag."));
}

void loop() {
  uint8_t uid[10];
  uint8_t uidLength = 0;
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100)) return;

  Serial.print(F("UID:"));
  for (uint8_t i = 0; i < uidLength; ++i) {
    Serial.print(' ');
    if (uid[i] < 0x10) Serial.print('0');
    Serial.print(uid[i], HEX);
  }
  Serial.println();

  if (sameUid(uid, uidLength)) {
    animateDoors();
  } else {
    showMessage(F("ACESSO NEGADO"));
    delay(900);
  }
  // Wait until the tag leaves the field to avoid repeated triggers.
  while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100)) delay(50);
  showMessage(F("Aproxime uma tag"));
}
