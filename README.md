# PockPlay

A pocket game console for the **ESP32-C3 SuperMini** with a **0.96" SSD1306 OLED** and just **two buttons** (UP / DOWN).

**Games:** CalcDodger, Dino Run, Pong, Frogger, Flappy, Snake, Breakout
**Tool:** TV-B-Gone (IR power-off blaster, USA/Canada and Europe/Asia code sets)

High scores are saved to flash and shown in the menu.

## Hardware

| Part | Connection |
|---|---|
| OLED SDA / SCL | GPIO5 / GPIO6 (3V3, GND) |
| UP button | GPIO3 to GND |
| DOWN button | GPIO4 to GND |
| Passive buzzer (optional) | GPIO10 to GND |

**Power:** 1S LiPo through a TP4056 protection module to the 5V pin via a switch. Never plug USB into the SuperMini while the battery is connected.

## INSTALATION METHODS

# 1. esptool by spacehuhn
1. go to releases page.
2. download the binary file.
3. open esp.huhn.me .
4. connect the esp
5. upload the binary
6. flash
7. wait
8. reboot
9. DONE!!!

## 2. Build

1. Install the Arduino IDE and the **ESP32 core 3.x** (needed for `tone()` and the LEDC API).
2. Install libraries: **Adafruit SSD1306** and **Adafruit GFX**.
3. Open `PockPlay_V0_2/PockPlay_V0_2.ino`, select **ESP32C3 Dev Module**, enable *USB CDC On Boot* if you want serial output, and upload.

## Controls

- **Menu:** UP / DOWN scroll, press **both** to start.
- **In game:** hold **both** for ~0.7 s to return to the menu.
- **TV-B-Gone:** UP = USA/Canada codes, DOWN = Europe/Asia codes. Hold both to stop.

| Game | UP | DOWN |
|---|---|---|
| CalcDodger | top lane | bottom lane |
| Dino Run | jump | duck / fast fall |
| Pong | paddle up | paddle down |
| Frogger | hop forward | hop back |
| Flappy | flap | dive |
| Snake | turn left | turn right |
| Breakout | paddle up | paddle down |
