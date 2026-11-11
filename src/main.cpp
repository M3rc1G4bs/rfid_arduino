#include <Adafruit_GFX.h>
#include <Adafruit_PN532.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <vector>

// --- PINOS I2C (OLED + PN532) ---
#define I2C_SDA 21
#define I2C_SCL 22

// --- PINOS SPI (Módulo MicroSD) ---
#define SD_CS   5
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23

// --- PINOS DOS BOTÕES ---
#define BTN_UP     32  // Anterior / Cima
#define BTN_SELECT 33  // Selecionar / Menu / Confirmar
#define BTN_DOWN   25  // Próximo / Baixo

Adafruit_PN532 nfc(0, 0, &Wire); // I2C; sem fios extras de IRQ/RESET
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Arquivo de persistência no SD
const char *CARDS_FILE = "/cards.txt";

// Lista dinâmica de UIDs autorizados na memória
std::vector<String> authorizedCards;
bool sdAvailable = false;
uint8_t idleFrame = 0;

// Estados da Interface
enum AppState {
  STATE_IDLE,
  STATE_MENU,
  STATE_ADD_CARD,
  STATE_LIST_CARDS,
  STATE_CONFIRM_DELETE
};

AppState currentState = STATE_IDLE;
int8_t menuSelection = 0;      // 0: Adicionar, 1: Listar/Remover, 2: Sair
int16_t cardListIndex = 0;     // Índice do cartão visualizado na lista
bool deleteConfirmChoice = false; // false: Cancelar, true: Confirmar

// Conversão de UID binário para String HEX (ex: "49 5B BC 61")
String uidToString(const uint8_t *uid, uint8_t length) {
  String str = "";
  for (uint8_t i = 0; i < length; ++i) {
    if (i > 0) str += " ";
    if (uid[i] < 0x10) str += "0";
    str += String(uid[i], HEX);
  }
  str.toUpperCase();
  return str;
}

// Checagem se um UID está na lista de autorizados
bool isUidAuthorized(const uint8_t *uid, uint8_t length) {
  String uidStr = uidToString(uid, length);
  for (const String &authorized : authorizedCards) {
    if (authorized.equalsIgnoreCase(uidStr)) {
      return true;
    }
  }
  return false;
}

// --- FUNÇÕES DE CARTÃO SD ---

void saveCardsToSD() {
  if (!sdAvailable) return;
  File file = SD.open(CARDS_FILE, FILE_WRITE);
  if (!file) {
    Serial.println(F("Erro ao abrir cards.txt para escrita no SD"));
    return;
  }
  for (const String &card : authorizedCards) {
    file.println(card);
  }
  file.close();
  Serial.println(F("Lista de cartões atualizada no SD."));
}

void loadCardsFromSD() {
  authorizedCards.clear();
  if (!sdAvailable) {
    // Se o SD não estiver inserido, usa um UID de contingência inicial
    authorizedCards.push_back("49 5B BC 61");
    return;
  }

  if (!SD.exists(CARDS_FILE)) {
    Serial.println(F("Arquivo cards.txt nao existe. Criando com UID padrao..."));
    authorizedCards.push_back("49 5B BC 61");
    saveCardsToSD();
    return;
  }

  File file = SD.open(CARDS_FILE, FILE_READ);
  if (!file) {
    Serial.println(F("Falha ao abrir cards.txt"));
    authorizedCards.push_back("49 5B BC 61");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      line.toUpperCase();
      authorizedCards.push_back(line);
    }
  }
  file.close();

  if (authorizedCards.empty()) {
    authorizedCards.push_back("49 5B BC 61");
    saveCardsToSD();
  }

  Serial.printf("Carregados %d cartoes do SD.\n", (int)authorizedCards.size());
}

// --- TRATAMENTO DOS BOTÕES (COM DEBOUNCE) ---

struct Button {
  uint8_t pin;
  bool lastState;
  unsigned long lastDebounceTime;
};

Button btnUp = {BTN_UP, HIGH, 0};
Button btnSelect = {BTN_SELECT, HIGH, 0};
Button btnDown = {BTN_DOWN, HIGH, 0};

bool checkButtonPressed(Button &btn) {
  bool reading = digitalRead(btn.pin);
  bool pressed = false;

  if (reading != btn.lastState) {
    if ((millis() - btn.lastDebounceTime) > 40) {
      if (btn.lastState == HIGH && reading == LOW) {
        pressed = true;
      }
      btn.lastState = reading;
      btn.lastDebounceTime = millis();
    }
  }
  return pressed;
}

// --- FUNÇÕES AUXILIARES DE DESENHO FOFO ---

void drawHeart(int16_t x, int16_t y) {
  display.fillCircle(x - 2, y + 2, 2, SSD1306_WHITE);
  display.fillCircle(x + 2, y + 2, 2, SSD1306_WHITE);
  display.fillTriangle(x - 4, y + 3, x + 4, y + 3, x, y + 7, SSD1306_WHITE);
}

void drawSparkle(int16_t x, int16_t y, bool big) {
  if (big) {
    display.drawFastHLine(x - 3, y, 7, SSD1306_WHITE);
    display.drawFastVLine(x, y - 3, 7, SSD1306_WHITE);
    display.drawPixel(x - 1, y - 1, SSD1306_WHITE);
    display.drawPixel(x + 1, y - 1, SSD1306_WHITE);
    display.drawPixel(x - 1, y + 1, SSD1306_WHITE);
    display.drawPixel(x + 1, y + 1, SSD1306_WHITE);
  } else {
    display.drawFastHLine(x - 1, y, 3, SSD1306_WHITE);
    display.drawFastVLine(x, y - 1, 3, SSD1306_WHITE);
  }
}

void drawPaw(int16_t x, int16_t y) {
  display.fillCircle(x, y + 2, 2, SSD1306_WHITE);
  display.drawPixel(x - 3, y - 1, SSD1306_WHITE);
  display.drawPixel(x - 1, y - 2, SSD1306_WHITE);
  display.drawPixel(x + 1, y - 2, SSD1306_WHITE);
  display.drawPixel(x + 3, y - 1, SSD1306_WHITE);
}

void drawHeader(const char *title = nullptr) {
  display.clearDisplay();
  display.drawFastHLine(14, 4, 100, SSD1306_WHITE);
  drawPaw(6, 4);
  drawPaw(121, 4);
  if (title) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    int16_t x = 64 - (strlen(title) * 3);
    display.setCursor(x, 8);
    display.print(title);
  }
}

// --- 1. ANIMAÇÃO DE ESPERA: GATINHO DORMINHOCO ---
void drawIdle(uint8_t frame) {
  drawHeader();

  int8_t breath = ((frame / 3) % 2 == 0) ? 0 : 1;
  int8_t tailWag = ((frame / 2) % 2 == 0) ? -2 : 2;
  int8_t earTwitch = (frame % 11 == 0) ? 2 : 0;

  // Almofada / Tapetinho
  display.drawRoundRect(26, 42, 76, 8, 4, SSD1306_WHITE);
  display.drawFastHLine(32, 47, 64, SSD1306_WHITE);

  // Corpo enrolado
  display.fillRoundRect(42, 25 + breath, 44, 21 - breath, 9, SSD1306_WHITE);
  display.drawRoundRect(44, 27 + breath, 40, 17 - breath, 7, SSD1306_BLACK);
  display.fillRoundRect(45, 28 + breath, 38, 15 - breath, 6, SSD1306_WHITE);

  // Cabecinha
  int16_t headX = 48;
  int16_t headY = 31 + breath;
  display.fillCircle(headX, headY, 12, SSD1306_WHITE);
  display.drawCircle(headX, headY, 12, SSD1306_BLACK);
  display.drawCircle(headX, headY, 11, SSD1306_WHITE);

  // Orelhas
  display.fillTriangle(headX - 11, headY - 4, headX - 8 + earTwitch, headY - 15,
                       headX - 2, headY - 9, SSD1306_WHITE);
  display.fillTriangle(headX + 2, headY - 9, headX + 8, headY - 15, headX + 11,
                       headY - 4, SSD1306_WHITE);
  display.drawPixel(headX - 7, headY - 10, SSD1306_BLACK);
  display.drawPixel(headX + 7, headY - 10, SSD1306_BLACK);

  // Olhinhos fechados felizes
  display.drawLine(headX - 7, headY, headX - 5, headY + 2, SSD1306_BLACK);
  display.drawLine(headX - 5, headY + 2, headX - 3, headY, SSD1306_BLACK);
  display.drawLine(headX + 3, headY, headX + 5, headY + 2, SSD1306_BLACK);
  display.drawLine(headX + 5, headY + 2, headX + 7, headY, SSD1306_BLACK);

  // Focinho e boquinha em "w"
  display.fillTriangle(headX - 1, headY + 3, headX + 1, headY + 3, headX,
                       headY + 4, SSD1306_BLACK);
  display.drawPixel(headX - 2, headY + 5, SSD1306_BLACK);
  display.drawPixel(headX - 1, headY + 6, SSD1306_BLACK);
  display.drawPixel(headX + 1, headY + 6, SSD1306_BLACK);
  display.drawPixel(headX + 2, headY + 5, SSD1306_BLACK);

  // Bigodinhos
  display.drawLine(headX - 13, headY + 2, headX - 9, headY + 3, SSD1306_WHITE);
  display.drawLine(headX - 13, headY + 5, headX - 9, headY + 5, SSD1306_WHITE);

  // Patinhas
  display.fillCircle(headX - 4, 44, 3, SSD1306_WHITE);
  display.drawCircle(headX - 4, 44, 3, SSD1306_BLACK);
  display.fillCircle(headX + 5, 44, 3, SSD1306_WHITE);
  display.drawCircle(headX + 5, 44, 3, SSD1306_BLACK);

  // Listras nas costas
  display.fillRoundRect(64, 26 + breath, 3, 6, 1, SSD1306_BLACK);
  display.fillRoundRect(70, 26 + breath, 3, 7, 1, SSD1306_BLACK);
  display.fillRoundRect(76, 27 + breath, 3, 5, 1, SSD1306_BLACK);

  // Rabinho
  display.drawLine(85, 39, 95, 34 + tailWag, SSD1306_WHITE);
  display.drawLine(85, 40, 95, 35 + tailWag, SSD1306_WHITE);
  display.fillCircle(96, 34 + tailWag, 2, SSD1306_WHITE);

  // Balõezinhos de "Zzz"
  uint8_t zStep = (frame / 2) % 4;
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  if (zStep >= 1) {
    display.setCursor(62, 16);
    display.print(F("z"));
  }
  if (zStep >= 2) {
    display.setCursor(71, 11);
    display.print(F("Z"));
  }
  if (zStep == 3) {
    display.setCursor(81, 7);
    display.print(F("Z"));
    drawSparkle(24, 22, true);
  } else {
    drawSparkle(24, 22, false);
  }

  // Texto inferior com status
  display.setCursor(22, 55);
  display.print(F("APROXIME TAG"));
  display.display();
}

// --- 2. ANIMAÇÃO DE ACESSO PERMITIDO: PORTA VERTICAL + COELHINHO FELIZ ---
void drawVerticalDoorAndBunny(uint8_t openHeight, uint8_t animTick) {
  drawHeader();

  int8_t jump = 0;
  if (openHeight >= 34) {
    jump = (animTick % 2 == 0) ? -3 : 0;
  }

  int16_t bx = 64;
  int16_t by = 38 + jump;

  int8_t earSpread = (jump < 0) ? 2 : 0;
  display.fillRoundRect(bx - 9 - earSpread, by - 22, 6, 15, 3, SSD1306_WHITE);
  display.fillRoundRect(bx + 3 + earSpread, by - 22, 6, 15, 3, SSD1306_WHITE);
  display.fillRoundRect(bx - 7 - earSpread, by - 19, 2, 9, 1, SSD1306_BLACK);
  display.fillRoundRect(bx + 5 + earSpread, by - 19, 2, 9, 1, SSD1306_BLACK);

  display.fillRoundRect(bx - 11, by + 4, 22, 14, 6, SSD1306_WHITE);
  display.fillCircle(bx, by, 11, SSD1306_WHITE);
  display.drawRoundRect(bx - 6, by + 7, 12, 9, 4, SSD1306_BLACK);

  display.drawPixel(bx - 8, by + 2, SSD1306_BLACK);
  display.drawPixel(bx - 7, by + 3, SSD1306_BLACK);
  display.drawPixel(bx + 8, by + 2, SSD1306_BLACK);
  display.drawPixel(bx + 7, by + 3, SSD1306_BLACK);

  if (openHeight >= 36 && (animTick % 2 == 0)) {
    display.drawLine(bx - 6, by - 1, bx - 4, by - 3, SSD1306_BLACK);
    display.drawLine(bx - 4, by - 3, bx - 2, by - 1, SSD1306_BLACK);
    display.drawLine(bx + 2, by - 1, bx + 4, by - 3, SSD1306_BLACK);
    display.drawLine(bx + 4, by - 3, bx + 6, by - 1, SSD1306_BLACK);
  } else {
    display.fillCircle(bx - 4, by - 2, 2, SSD1306_BLACK);
    display.fillCircle(bx + 4, by - 2, 2, SSD1306_BLACK);
    display.drawPixel(bx - 5, by - 3, SSD1306_WHITE);
    display.drawPixel(bx + 3, by - 3, SSD1306_WHITE);
  }

  display.fillTriangle(bx - 1, by + 1, bx + 1, by + 1, bx, by + 2, SSD1306_BLACK);
  display.drawLine(bx - 2, by + 4, bx, by + 3, SSD1306_BLACK);
  display.drawLine(bx, by + 3, bx + 2, by + 4, SSD1306_BLACK);

  int8_t armY = (jump < 0) ? -4 : 0;
  display.fillCircle(bx - 12, by + 6 + armY, 3, SSD1306_WHITE);
  display.fillCircle(bx + 12, by + 6 + armY, 3, SSD1306_WHITE);

  display.fillRoundRect(bx - 9, 51 + (jump / 2), 6, 4, 2, SSD1306_WHITE);
  display.fillRoundRect(bx + 3, 51 + (jump / 2), 6, 4, 2, SSD1306_WHITE);

  uint8_t doorHeight = (openHeight >= 40) ? 0 : (40 - openHeight);
  if (doorHeight > 0) {
    uint8_t doorBottomY = 14 + doorHeight;
    display.fillRect(32, 14, 64, doorHeight, SSD1306_BLACK);
    display.fillRect(33, 14, 62, doorHeight, SSD1306_WHITE);

    for (int16_t slatY = doorBottomY - 8; slatY >= 15; slatY -= 8) {
      display.drawFastHLine(35, slatY, 58, SSD1306_BLACK);
      display.drawPixel(37, slatY + 3, SSD1306_BLACK);
      display.drawPixel(90, slatY + 3, SSD1306_BLACK);
    }
    if (doorHeight >= 6) {
      display.fillRoundRect(56, doorBottomY - 5, 16, 3, 1, SSD1306_BLACK);
    }
    display.drawFastHLine(33, doorBottomY - 1, 62, SSD1306_BLACK);
  }

  display.drawRect(28, 10, 72, 46, SSD1306_WHITE);
  display.drawRect(31, 13, 66, 42, SSD1306_WHITE);
  display.drawFastHLine(18, 56, 92, SSD1306_WHITE);
  display.drawFastHLine(14, 58, 100, SSD1306_WHITE);

  if (openHeight > 22) {
    int8_t floatY = (animTick % 2 == 0) ? -2 : 1;
    drawHeart(14, 24 + floatY);
    drawHeart(113, 24 - floatY);
    drawSparkle(16, 44, (animTick % 2 == 0));
    drawSparkle(111, 44, (animTick % 2 != 0));
  }

  display.display();
}

void animateGranted() {
  uint8_t tick = 0;
  for (uint8_t h = 0; h <= 40; h += 2) {
    drawVerticalDoorAndBunny(h, tick++);
    delay(30);
  }
  for (uint8_t i = 0; i < 10; ++i) {
    drawVerticalDoorAndBunny(40, tick++);
    delay(120);
  }
}

// --- 3. ANIMAÇÃO DE ACESSO NEGADO: URSINHO TRISTE ---
void drawDeniedBear(uint8_t frame) {
  drawHeader();

  const int8_t shakeOffsets[4] = {-3, 0, 3, 0};
  int8_t shakeX = shakeOffsets[frame % 4];
  int8_t bobY = (frame % 2 == 0) ? 0 : 1;

  int16_t cx = 64 + shakeX;
  int16_t cy = 27 + bobY;

  display.fillCircle(cx - 16, cy - 9, 6, SSD1306_WHITE);
  display.fillCircle(cx + 16, cy - 9, 6, SSD1306_WHITE);
  display.fillCircle(cx - 16, cy - 9, 3, SSD1306_BLACK);
  display.fillCircle(cx + 16, cy - 9, 3, SSD1306_BLACK);

  display.fillCircle(cx, cy, 17, SSD1306_WHITE);
  display.drawCircle(cx, cy, 17, SSD1306_BLACK);
  display.drawCircle(cx, cy, 16, SSD1306_WHITE);

  display.drawLine(cx - 10, cy - 7, cx - 4, cy - 5, SSD1306_BLACK);
  display.drawLine(cx + 10, cy - 7, cx + 4, cy - 5, SSD1306_BLACK);

  display.drawLine(cx - 9, cy - 3, cx - 5, cy + 1, SSD1306_BLACK);
  display.drawLine(cx - 9, cy + 1, cx - 5, cy - 3, SSD1306_BLACK);
  display.drawLine(cx + 5, cy - 3, cx + 9, cy + 1, SSD1306_BLACK);
  display.drawLine(cx + 5, cy + 1, cx + 9, cy - 3, SSD1306_BLACK);

  display.drawRoundRect(cx - 7, cy + 2, 14, 10, 4, SSD1306_BLACK);
  display.fillTriangle(cx - 2, cy + 4, cx + 2, cy + 4, cx, cy + 6, SSD1306_BLACK);

  display.drawLine(cx - 3, cy + 9, cx, cy + 7, SSD1306_BLACK);
  display.drawLine(cx, cy + 7, cx + 3, cy + 9, SSD1306_BLACK);

  int8_t dropY = (frame % 4) * 2;
  display.fillCircle(cx + 21, cy - 4 + dropY, 2, SSD1306_WHITE);
  display.fillTriangle(cx + 19, cy - 4 + dropY, cx + 23, cy - 4 + dropY,
                       cx + 21, cy - 9 + dropY, SSD1306_WHITE);

  display.fillRoundRect(44, 43, 40, 17, 7, SSD1306_WHITE);
  display.drawRoundRect(46, 45, 36, 13, 5, SSD1306_BLACK);

  display.drawLine(50, 46, 78, 56, SSD1306_BLACK);
  display.drawLine(50, 47, 78, 57, SSD1306_BLACK);
  display.drawLine(78, 46, 50, 56, SSD1306_BLACK);
  display.drawLine(78, 47, 50, 57, SSD1306_BLACK);

  if (frame % 2 == 0) {
    display.drawLine(12, 24, 22, 34, SSD1306_WHITE);
    display.drawLine(22, 24, 12, 34, SSD1306_WHITE);
    display.drawLine(106, 24, 116, 34, SSD1306_WHITE);
    display.drawLine(116, 24, 106, 34, SSD1306_WHITE);
  }

  display.display();
}

void animateDenied() {
  for (uint8_t frame = 0; frame < 12; ++frame) {
    drawDeniedBear(frame);
    delay(110);
  }
  delay(400);
}

// --- TELAS DO GERENCIADOR DE CARTÕES ---

void drawMenu() {
  drawHeader("CONFIG CARTOES");

  const char *options[] = {"1. Adicionar Tag", "2. Ver / Excluir", "3. Sair"};
  for (int8_t i = 0; i < 3; ++i) {
    int16_t y = 20 + (i * 14);
    if (menuSelection == i) {
      display.fillRoundRect(8, y - 2, 112, 13, 3, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(14, y + 1);
      display.print(options[i]);
      drawPaw(110, y + 4);
    } else {
      display.drawRoundRect(8, y - 2, 112, 13, 3, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(14, y + 1);
      display.print(options[i]);
    }
  }
  display.display();
}

void drawAddCardScreen() {
  drawHeader("NOVO CARTAO");

  display.drawRoundRect(8, 18, 112, 42, 4, SSD1306_WHITE);
  drawSparkle(16, 26, true);
  drawSparkle(111, 26, false);

  display.setTextColor(SSD1306_WHITE);
  display.setCursor(18, 26);
  display.print(F("Aproxime a tag"));

  display.setCursor(20, 39);
  display.print(F("para cadastrar"));

  display.setCursor(14, 51);
  display.print(F("[Voltar: Btn Cima]"));

  display.display();
}

void drawCardListScreen() {
  drawHeader("LISTA DE TAGS");

  if (authorizedCards.empty()) {
    display.setCursor(20, 30);
    display.setTextColor(SSD1306_WHITE);
    display.print(F("Nenhum cartao!"));
    display.setCursor(15, 48);
    display.print(F("[Sel para Voltar]"));
    display.display();
    return;
  }

  if (cardListIndex >= (int16_t)authorizedCards.size()) {
    cardListIndex = authorizedCards.size() - 1;
  }
  if (cardListIndex < 0) {
    cardListIndex = 0;
  }

  // Caixa da tag atual
  display.drawRoundRect(8, 18, 112, 42, 4, SSD1306_WHITE);
  
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(14, 22);
  display.printf("Tag %d de %d:", cardListIndex + 1, (int)authorizedCards.size());

  // UID com destaque
  display.fillRoundRect(12, 32, 104, 13, 2, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(16, 35);
  display.print(authorizedCards[cardListIndex]);

  // Dica inferior
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 48);
  display.print(F("SEL:Excluir ^v:Naveg"));

  display.display();
}

void drawConfirmDeleteScreen() {
  drawHeader("EXCLUIR CARTAO?");

  display.drawRoundRect(8, 18, 112, 42, 4, SSD1306_WHITE);

  display.setTextColor(SSD1306_WHITE);
  display.setCursor(14, 22);
  display.print(authorizedCards[cardListIndex]);

  // Botão Cancelar
  if (!deleteConfirmChoice) {
    display.fillRoundRect(14, 38, 46, 14, 3, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(18, 41);
    display.print(F("< Nao"));
  } else {
    display.drawRoundRect(14, 38, 46, 14, 3, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(18, 41);
    display.print(F("  Nao"));
  }

  // Botão Confirmar
  if (deleteConfirmChoice) {
    display.fillRoundRect(68, 38, 46, 14, 3, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(72, 41);
    display.print(F("Sim >"));
  } else {
    display.drawRoundRect(68, 38, 46, 14, 3, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(72, 41);
    display.print(F("  Sim"));
  }

  display.display();
}

void showMessage(const char *msg1, const char *msg2, bool success = true) {
  drawHeader();
  display.drawRoundRect(8, 18, 112, 40, 4, SSD1306_WHITE);
  if (success) {
    drawHeart(20, 30);
    drawHeart(107, 30);
  } else {
    drawSparkle(20, 30, false);
    drawSparkle(107, 30, false);
  }
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(24, 25);
  display.print(msg1);
  if (msg2) {
    display.setCursor(24, 38);
    display.print(msg2);
  }
  display.display();
  delay(1200);
}

// --- SETUP ---

void setup() {
  Serial.begin(115200);

  // Configuração dos botões com PULL-UP interno
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED nao encontrado (tente 0x3D)"));
    while (true) delay(100);
  }

  drawHeader();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(17, 24);
  display.print(F("Iniciando Sistema..."));
  display.display();

  // Inicialização do Módulo SD com SPI
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println(F("Falha ao inicializar Cartao SD ou SD ausente."));
    sdAvailable = false;
    display.setCursor(14, 40);
    display.print(F("SD: Nao Detectado"));
  } else {
    Serial.println(F("Cartao SD montado com sucesso!"));
    sdAvailable = true;
    display.setCursor(14, 40);
    display.print(F("SD: Conectado!"));
  }
  display.display();
  delay(800);

  // Carrega lista de cartões do SD
  loadCardsFromSD();

  // Inicializa o leitor PN532
  nfc.begin();
  if (!nfc.getFirmwareVersion()) {
    Serial.println(F("PN532 nao encontrado. Verifique pinagem e modo I2C."));
    drawHeader();
    display.setCursor(9, 29);
    display.print(F("PN532 nao achado"));
    display.setCursor(9, 41);
    display.print(F("Verifique o I2C"));
    display.display();
    while (true) delay(100);
  }
  nfc.SAMConfig();
  drawIdle(0);
  Serial.println(F("Sistema pronto!"));
}

// --- LOOP PRINCIPAL ---

void loop() {
  bool upPressed = checkButtonPressed(btnUp);
  bool selPressed = checkButtonPressed(btnSelect);
  bool downPressed = checkButtonPressed(btnDown);

  // MÁQUINA DE ESTADOS DA INTERFACE
  switch (currentState) {
    case STATE_IDLE: {
      // Se pressionar o botão Select/Menu, entra no modo de gerenciamento
      if (selPressed) {
        currentState = STATE_MENU;
        menuSelection = 0;
        drawMenu();
        return;
      }

      // Leitura não bloqueante (timeout rápido de 80ms)
      uint8_t uid[10];
      uint8_t uidLength = 0;
      if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 80)) {
        drawIdle(idleFrame++);
        return;
      }

      String cardHex = uidToString(uid, uidLength);
      Serial.print(F("Tag aproximada: "));
      Serial.println(cardHex);

      if (isUidAuthorized(uid, uidLength)) {
        animateGranted();
      } else {
        animateDenied();
      }

      // Aguarda o cartão se afastar
      while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 80)) {
        delay(50);
      }
      drawIdle(idleFrame++);
      break;
    }

    case STATE_MENU: {
      if (upPressed) {
        menuSelection--;
        if (menuSelection < 0) menuSelection = 2;
        drawMenu();
      } else if (downPressed) {
        menuSelection++;
        if (menuSelection > 2) menuSelection = 0;
        drawMenu();
      } else if (selPressed) {
        if (menuSelection == 0) {
          currentState = STATE_ADD_CARD;
          drawAddCardScreen();
        } else if (menuSelection == 1) {
          currentState = STATE_LIST_CARDS;
          cardListIndex = 0;
          drawCardListScreen();
        } else if (menuSelection == 2) {
          currentState = STATE_IDLE;
          drawIdle(idleFrame);
        }
      }
      break;
    }

    case STATE_ADD_CARD: {
      // Botão UP ou DOWN permite cancelar e voltar ao menu
      if (upPressed || downPressed) {
        currentState = STATE_MENU;
        drawMenu();
        return;
      }

      // Tenta ler uma tag para cadastrar
      uint8_t uid[10];
      uint8_t uidLength = 0;
      if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 150)) {
        String newUid = uidToString(uid, uidLength);
        Serial.print(F("Tentando cadastrar tag: "));
        Serial.println(newUid);

        if (isUidAuthorized(uid, uidLength)) {
          showMessage("Tag ja existe!", newUid.c_str(), false);
        } else {
          authorizedCards.push_back(newUid);
          saveCardsToSD();
          showMessage("Tag Salva no SD!", newUid.c_str(), true);
        }

        // Aguarda retirar o cartão
        while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100)) {
          delay(50);
        }

        currentState = STATE_MENU;
        drawMenu();
      }
      break;
    }

    case STATE_LIST_CARDS: {
      if (authorizedCards.empty()) {
        if (selPressed || upPressed || downPressed) {
          currentState = STATE_MENU;
          drawMenu();
        }
        return;
      }

      if (upPressed) {
        if (cardListIndex > 0) {
          cardListIndex--;
          drawCardListScreen();
        }
      } else if (downPressed) {
        if (cardListIndex < (int16_t)authorizedCards.size() - 1) {
          cardListIndex++;
          drawCardListScreen();
        }
      } else if (selPressed) {
        currentState = STATE_CONFIRM_DELETE;
        deleteConfirmChoice = false; // Inicia em "Não" por segurança
        drawConfirmDeleteScreen();
      }
      break;
    }

    case STATE_CONFIRM_DELETE: {
      if (upPressed || downPressed) {
        deleteConfirmChoice = !deleteConfirmChoice;
        drawConfirmDeleteScreen();
      } else if (selPressed) {
        if (deleteConfirmChoice) {
          // Confirmado exclusão
          String removedUid = authorizedCards[cardListIndex];
          authorizedCards.erase(authorizedCards.begin() + cardListIndex);
          saveCardsToSD();
          showMessage("Tag Excluida!", removedUid.c_str(), true);
          if (cardListIndex >= (int16_t)authorizedCards.size()) {
            cardListIndex = authorizedCards.empty() ? 0 : authorizedCards.size() - 1;
          }
          currentState = STATE_LIST_CARDS;
          drawCardListScreen();
        } else {
          // Cancelado
          currentState = STATE_LIST_CARDS;
          drawCardListScreen();
        }
      }
      break;
    }
  }
}
