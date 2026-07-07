/*
 * test_tft_minimal.ino — 最简 TFT 初始化测试
 * 不加传感器、不加 UI，只测 TFT_eSPI + GC9A01
 *
 * 如果这个也崩 → User_Setup.h 或 TFT_eSPI 配置有问题
 * 如果这个正常 → 主项目代码有其他问题
 */

#include <TFT_eSPI.h>

static TFT_eSPI tft;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== TFT Minimal Test ===");

    Serial.println("[1] tft.begin()...");
    Serial.flush();
    tft.begin();

    Serial.println("[2] setRotation...");
    Serial.flush();
    tft.setRotation(0);

    Serial.println("[3] fillScreen...");
    Serial.flush();
    tft.fillScreen(0x0000); // black

    Serial.println("[4] draw text...");
    Serial.flush();
    tft.setTextColor(0xFFFF, 0x0000); // white on black
    tft.drawString("OK", 100, 100, 4);

    Serial.println("=== TFT OK ===");
    Serial.println("If you see 'OK' on screen, TFT works.");
}

void loop() {
    delay(1000);
}
