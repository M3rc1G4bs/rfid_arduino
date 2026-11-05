#include <Adafruit_GFX.h>
#include <Adafruit_PN532.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

Adafruit_PN532 nfc(0, 0, &Wire); // I2C; sem fios extras de IRQ/RESET
Adafruit_SSD1306 display(128, 64, &Wire, -1);

const uint8_t AUTHORIZED_UID[] = {0x7F, 0xD5, 0x50, 0xF6};
const uint8_t AUTHORIZED_UID_LENGTH = sizeof(AUTHORIZED_UID);
uint8_t idleFrame = 0;

bool sameUid(const uint8_t *uid, uint8_t length) {
  if (length != AUTHORIZED_UID_LENGTH)
    return false;
  for (uint8_t i = 0; i < length; ++i) {
    if (uid[i] != AUTHORIZED_UID[i])
      return false;
  }
  return true;
}

// --- FUNÇÕES AUXILIARES DE DESENHO FOFO ---

// Desenha um pequeno coração (7x6 pixels) a partir do topo central (x, y)
void drawHeart(int16_t x, int16_t y) {
  display.fillCircle(x - 2, y + 2, 2, SSD1306_WHITE);
  display.fillCircle(x + 2, y + 2, 2, SSD1306_WHITE);
  display.fillTriangle(x - 4, y + 3, x + 4, y + 3, x, y + 7, SSD1306_WHITE);
}

// Desenha uma estrelinha brilhante que alterna de tamanho
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

// Desenha uma patinha decorativa nos cantos
void drawPaw(int16_t x, int16_t y) {
  display.fillCircle(x, y + 2, 2, SSD1306_WHITE);
  display.drawPixel(x - 3, y - 1, SSD1306_WHITE);
  display.drawPixel(x - 1, y - 2, SSD1306_WHITE);
  display.drawPixel(x + 1, y - 2, SSD1306_WHITE);
  display.drawPixel(x + 3, y - 1, SSD1306_WHITE);
}

void drawHeader() {
  display.clearDisplay();
  display.drawFastHLine(14, 4, 100, SSD1306_WHITE);
  drawPaw(6, 4);
  drawPaw(121, 4);
}

// --- 1. ANIMAÇÃO DE ESPERA: GATINHO DORMINHOCO ---
void drawIdle(uint8_t frame) {
  drawHeader();

  // Respiração suave (sobe e desce a cada 4 frames)
  int8_t breath = ((frame / 3) % 2 == 0) ? 0 : 1;
  // Movimento do rabinho
  int8_t tailWag = ((frame / 2) % 2 == 0) ? -2 : 2;
  // Tremidinha na orelha ocasional
  int8_t earTwitch = (frame % 11 == 0) ? 2 : 0;

  // Almofada / Tapetinho onde o gatinho dorme
  display.drawRoundRect(26, 42, 76, 8, 4, SSD1306_WHITE);
  display.drawFastHLine(32, 47, 64, SSD1306_WHITE);

  // Corpo do gatinho enrolado (respira)
  display.fillRoundRect(42, 25 + breath, 44, 21 - breath, 9, SSD1306_WHITE);
  display.drawRoundRect(44, 27 + breath, 40, 17 - breath, 7,
                        SSD1306_BLACK); // Contorno interno
  display.fillRoundRect(45, 28 + breath, 38, 15 - breath, 6, SSD1306_WHITE);

  // Cabecinha do gatinho (lado esquerdo do corpo)
  int16_t headX = 48;
  int16_t headY = 31 + breath;
  display.fillCircle(headX, headY, 12, SSD1306_WHITE);
  display.drawCircle(headX, headY, 12, SSD1306_BLACK);
  display.drawCircle(headX, headY, 11, SSD1306_WHITE);

  // Orelhas pontudinhas
  display.fillTriangle(headX - 11, headY - 4, headX - 8 + earTwitch, headY - 15,
                       headX - 2, headY - 9, SSD1306_WHITE);
  display.fillTriangle(headX + 2, headY - 9, headX + 8, headY - 15, headX + 11,
                       headY - 4, SSD1306_WHITE);
  // Detalhe interno da orelha
  display.drawPixel(headX - 7, headY - 10, SSD1306_BLACK);
  display.drawPixel(headX + 7, headY - 10, SSD1306_BLACK);

  // Olhinhos fechados felizes/dormindo (arcos para baixo)
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

  // Patinhas brancas apoiadas na frente
  display.fillCircle(headX - 4, 44, 3, SSD1306_WHITE);
  display.drawCircle(headX - 4, 44, 3, SSD1306_BLACK);
  display.fillCircle(headX + 5, 44, 3, SSD1306_WHITE);
  display.drawCircle(headX + 5, 44, 3, SSD1306_BLACK);

  // Listras nas costas do gatinho
  display.fillRoundRect(64, 26 + breath, 3, 6, 1, SSD1306_BLACK);
  display.fillRoundRect(70, 26 + breath, 3, 7, 1, SSD1306_BLACK);
  display.fillRoundRect(76, 27 + breath, 3, 5, 1, SSD1306_BLACK);

  // Rabinho animado na direita
  display.drawLine(85, 39, 95, 34 + tailWag, SSD1306_WHITE);
  display.drawLine(85, 40, 95, 35 + tailWag, SSD1306_WHITE);
  display.fillCircle(96, 34 + tailWag, 2, SSD1306_WHITE);

  // Balõezinhos de "Zzz" subindo
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

  // Texto inferior
  display.setCursor(28, 55);
  display.print(F("APROXIME TAG"));
  display.display();
}

// --- 2. ANIMAÇÃO DE ACESSO PERMITIDO: PORTA VERTICAL + COELHINHO FELIZ ---
// openHeight varia de 0 (porta totalmente fechada) até 40 (porta totalmente
// aberta no topo)
void drawVerticalDoorAndBunny(uint8_t openHeight, uint8_t animTick) {
  drawHeader();

  // Pulo do coelhinho quando a porta já está quase toda aberta
  int8_t jump = 0;
  if (openHeight >= 34) {
    jump = (animTick % 2 == 0) ? -3 : 0;
  }

  // --- CAMADA 1: O COELHINHO FOFO LÁ DENTRO (Fundo da porta) ---
  int16_t bx = 64;
  int16_t by = 38 + jump;

  // Orelhas compridas do coelhinho (mexem quando ele pula)
  int8_t earSpread = (jump < 0) ? 2 : 0;
  display.fillRoundRect(bx - 9 - earSpread, by - 22, 6, 15, 3, SSD1306_WHITE);
  display.fillRoundRect(bx + 3 + earSpread, by - 22, 6, 15, 3, SSD1306_WHITE);
  display.fillRoundRect(bx - 7 - earSpread, by - 19, 2, 9, 1,
                        SSD1306_BLACK); // Miolo da orelha
  display.fillRoundRect(bx + 5 + earSpread, by - 19, 2, 9, 1, SSD1306_BLACK);

  // Corpinho gordinho e cabecinha
  display.fillRoundRect(bx - 11, by + 4, 22, 14, 6, SSD1306_WHITE);
  display.fillCircle(bx, by, 11, SSD1306_WHITE);
  // Barriguinha branca contornada
  display.drawRoundRect(bx - 6, by + 7, 12, 9, 4, SSD1306_BLACK);

  // Bochechas fofas (pontinhos nas laterais)
  display.drawPixel(bx - 8, by + 2, SSD1306_BLACK);
  display.drawPixel(bx - 7, by + 3, SSD1306_BLACK);
  display.drawPixel(bx + 8, by + 2, SSD1306_BLACK);
  display.drawPixel(bx + 7, by + 3, SSD1306_BLACK);

  // Olhinhos: quando a porta abre toda, ficam em arco feliz (^ ^), antes disso
  // ficam brilhantes
  if (openHeight >= 36 && (animTick % 2 == 0)) {
    display.drawLine(bx - 6, by - 1, bx - 4, by - 3, SSD1306_BLACK);
    display.drawLine(bx - 4, by - 3, bx - 2, by - 1, SSD1306_BLACK);
    display.drawLine(bx + 2, by - 1, bx + 4, by - 3, SSD1306_BLACK);
    display.drawLine(bx + 4, by - 3, bx + 6, by - 1, SSD1306_BLACK);
  } else {
    display.fillCircle(bx - 4, by - 2, 2, SSD1306_BLACK);
    display.fillCircle(bx + 4, by - 2, 2, SSD1306_BLACK);
    display.drawPixel(bx - 5, by - 3, SSD1306_WHITE); // Brilho no olho
    display.drawPixel(bx + 3, by - 3, SSD1306_WHITE);
  }

  // Narizinho e sorriso
  display.fillTriangle(bx - 1, by + 1, bx + 1, by + 1, bx, by + 2,
                       SSD1306_BLACK);
  display.drawLine(bx - 2, by + 4, bx, by + 3, SSD1306_BLACK);
  display.drawLine(bx, by + 3, bx + 2, by + 4, SSD1306_BLACK);

  // Bracinhos levantados comemorando!
  int8_t armY = (jump < 0) ? -4 : 0;
  display.fillCircle(bx - 12, by + 6 + armY, 3, SSD1306_WHITE);
  display.fillCircle(bx + 12, by + 6 + armY, 3, SSD1306_WHITE);

  // Pezinhos
  display.fillRoundRect(bx - 9, 51 + (jump / 2), 6, 4, 2, SSD1306_WHITE);
  display.fillRoundRect(bx + 3, 51 + (jump / 2), 6, 4, 2, SSD1306_WHITE);

  // --- CAMADA 2: A PORTA VERTICAL (EIXO Y - SOBE PARA ABRIR) ---
  // A abertura da porta vai de Y = 14 (topo) até Y = 54 (chão), altura total =
  // 40px
  uint8_t doorHeight = (openHeight >= 40) ? 0 : (40 - openHeight);

  if (doorHeight > 0) {
    uint8_t doorBottomY = 14 + doorHeight;

    // Preenchimento opaco preto atrás da porta para esconder o coelhinho
    // enquanto está fechada
    display.fillRect(32, 14, 64, doorHeight, SSD1306_BLACK);

    // Folha da porta branca que desliza para cima
    display.fillRect(33, 14, 62, doorHeight, SSD1306_WHITE);

    // Gomos horizontais da porta de correr vertical (efeito portão/madeira)
    for (int16_t slatY = doorBottomY - 8; slatY >= 15; slatY -= 8) {
      display.drawFastHLine(35, slatY, 58, SSD1306_BLACK);
      // Rebites fofos nas laterais de cada gomo
      display.drawPixel(37, slatY + 3, SSD1306_BLACK);
      display.drawPixel(90, slatY + 3, SSD1306_BLACK);
    }

    // Puxador central na base inferior da porta (sobe junto com a porta!)
    if (doorHeight >= 6) {
      display.fillRoundRect(56, doorBottomY - 5, 16, 3, 1, SSD1306_BLACK);
    }
    // Borda inferior reforçada da porta
    display.drawFastHLine(33, doorBottomY - 1, 62, SSD1306_BLACK);
  }

  // --- CAMADA 3: BATENTE / MOLDURA DA PORTA E EFEITOS DE FESTA ---
  display.drawRect(28, 10, 72, 46, SSD1306_WHITE);
  display.drawRect(31, 13, 66, 42, SSD1306_WHITE);
  // Soleira / Chão
  display.drawFastHLine(18, 56, 92, SSD1306_WHITE);
  display.drawFastHLine(14, 58, 100, SSD1306_WHITE);

  // Corações e brilhos ao redor quando a porta se abre
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
  // 1. A porta abre deslizando verticalmente de baixo para cima (0 a 40 pixels)
  for (uint8_t h = 0; h <= 40; h += 2) {
    drawVerticalDoorAndBunny(h, tick++);
    delay(30);
  }
  // 2. Com a porta toda aberta no topo, o coelhinho comemora pulando com
  // corações!
  for (uint8_t i = 0; i < 10; ++i) {
    drawVerticalDoorAndBunny(40, tick++);
    delay(120);
  }
}

// --- 3. ANIMAÇÃO DE ACESSO NEGADO: URSINHO BRAVINHO / TRISTE DIZENDO "NÃO" ---
void drawDeniedBear(uint8_t frame) {
  drawHeader();

  // O ursinho balança a cabeça para a esquerda e para a direita ("Não, não!")
  const int8_t shakeOffsets[4] = {-3, 0, 3, 0};
  int8_t shakeX = shakeOffsets[frame % 4];
  int8_t bobY = (frame % 2 == 0) ? 0 : 1;

  int16_t cx = 64 + shakeX;
  int16_t cy = 27 + bobY;

  // Orelhas redondinhas caídas/tristes
  display.fillCircle(cx - 16, cy - 9, 6, SSD1306_WHITE);
  display.fillCircle(cx + 16, cy - 9, 6, SSD1306_WHITE);
  display.fillCircle(cx - 16, cy - 9, 3, SSD1306_BLACK);
  display.fillCircle(cx + 16, cy - 9, 3, SSD1306_BLACK);

  // Cabecinha redonda do ursinho
  display.fillCircle(cx, cy, 17, SSD1306_WHITE);
  display.drawCircle(cx, cy, 17, SSD1306_BLACK);
  display.drawCircle(cx, cy, 16, SSD1306_WHITE);

  // Sobrancelhas franzidas
  display.drawLine(cx - 10, cy - 7, cx - 4, cy - 5, SSD1306_BLACK);
  display.drawLine(cx + 10, cy - 7, cx + 4, cy - 5, SSD1306_BLACK);

  // Olhinhos em "X" fofos (> <)
  display.drawLine(cx - 9, cy - 3, cx - 5, cy + 1, SSD1306_BLACK);
  display.drawLine(cx - 9, cy + 1, cx - 5, cy - 3, SSD1306_BLACK);
  display.drawLine(cx + 5, cy - 3, cx + 9, cy + 1, SSD1306_BLACK);
  display.drawLine(cx + 5, cy + 1, cx + 9, cy - 3, SSD1306_BLACK);

  // Focinho oval e narizinho
  display.drawRoundRect(cx - 7, cy + 2, 14, 10, 4, SSD1306_BLACK);
  display.fillTriangle(cx - 2, cy + 4, cx + 2, cy + 4, cx, cy + 6,
                       SSD1306_BLACK);

  // Boquinha fazendo bico triste / tremelicando
  display.drawLine(cx - 3, cy + 9, cx, cy + 7, SSD1306_BLACK);
  display.drawLine(cx, cy + 7, cx + 3, cy + 9, SSD1306_BLACK);

  // Gotinha de suor cômica caindo na lateral da testa
  int8_t dropY = (frame % 4) * 2;
  display.fillCircle(cx + 21, cy - 4 + dropY, 2, SSD1306_WHITE);
  display.fillTriangle(cx + 19, cy - 4 + dropY, cx + 23, cy - 4 + dropY,
                       cx + 21, cy - 9 + dropY, SSD1306_WHITE);

  // Corpinho e patinhas cruzadas formando um "X" de bloqueio no peito
  display.fillRoundRect(44, 43, 40, 17, 7, SSD1306_WHITE);
  display.drawRoundRect(46, 45, 36, 13, 5, SSD1306_BLACK);

  // Braços cruzados em X
  display.drawLine(50, 46, 78, 56, SSD1306_BLACK);
  display.drawLine(50, 47, 78, 57, SSD1306_BLACK);
  display.drawLine(78, 46, 50, 56, SSD1306_BLACK);
  display.drawLine(78, 47, 50, 57, SSD1306_BLACK);

  // Plaquinhas de "X" piscando nos lados da tela
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

void printUid(const uint8_t *uid, uint8_t length) {
  Serial.print(F("UID:"));
  for (uint8_t i = 0; i < length; ++i) {
    Serial.print(' ');
    if (uid[i] < 0x10)
      Serial.print('0');
    Serial.print(uid[i], HEX);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED not found (try address 0x3D)"));
    while (true)
      delay(100);
  }
  drawHeader();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(17, 31);
  display.print(F("Starting NFC reader"));
  display.display();

  nfc.begin();
  if (!nfc.getFirmwareVersion()) {
    Serial.println(F("PN532 not found. Check I2C mode and wiring."));
    drawHeader();
    display.setCursor(9, 29);
    display.print(F("PN532 not found"));
    display.setCursor(9, 41);
    display.print(F("Check I2C wiring"));
    display.display();
    while (true)
      delay(100);
  }
  nfc.SAMConfig();
  drawIdle(0);
  Serial.println(F("PN532 ready. Tap a card."));
}

void loop() {
  uint8_t uid[10];
  uint8_t uidLength = 0;
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100)) {
    drawIdle(idleFrame++);
    return;
  }

  printUid(uid, uidLength);
  if (sameUid(uid, uidLength))
    animateGranted();
  else
    animateDenied();

  // Aguarda o cartão sair do leitor antes de voltar para o gatinho dormindo
  while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100))
    delay(50);
  drawIdle(idleFrame++);
}