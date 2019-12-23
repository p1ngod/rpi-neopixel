#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#ifdef __AVR__
  #include <avr/power.h>
#endif

#define AVG_SPEED 16
#define AVG_WIDTH 16
#define STRIPS_COUNT 3
#define SERIAL_SPEED 115200

typedef void (* Animation)(Adafruit_NeoPixel&, uint32_t, uint32_t, uint8_t, uint8_t, uint32_t);

static Adafruit_NeoPixel *strips[STRIPS_COUNT] = {
  new Adafruit_NeoPixel(600, 2, NEO_GRBW + NEO_KHZ800),
  new Adafruit_NeoPixel(300, 3, NEO_GRBW + NEO_KHZ800),
  new Adafruit_NeoPixel(300, 4, NEO_GRBW + NEO_KHZ800)
};

struct AnimationConfig {
  Animation animation;
  uint32_t color1;
  uint32_t color2;
  uint8_t speed;
  uint8_t width;
};
static AnimationConfig animationConfigs[STRIPS_COUNT];

const size_t capacity = JSON_ARRAY_SIZE(3) + 3*JSON_OBJECT_SIZE(5) + 90;
StaticJsonDocument<capacity> jsonDoc;
DeserializationError jsonError;

uint32_t startTick = 0;
uint32_t currentTick = 0;

/* A PROGMEM (flash mem) table containing 8-bit unsigned random values (0-255).
   Copy & paste this snippet into a Python REPL to regenerate:
import random
random.seed()
for i, x in enumerate(random.sample(range(256), 256)):
    print("{:3},".format(x)),
    if i&15 == 15: print
*/
static const uint8_t PROGMEM RandomTable[256] = {
   84,  18, 210,  35, 120, 155, 102,  54,  89,  63,  76,   1,   4, 114,  19,  71,
  225, 209, 137, 162,  64, 193, 132, 226, 110, 103,  41,  65,  14,  87, 247, 170,
  228,  58, 235, 207, 164, 143,  29, 168,  94, 238, 166, 134, 111, 131,  43, 122,
  218,  61,  31, 216, 202,  91,   6, 113, 163,  33, 229, 241,  36, 159, 178,  38,
  116, 107, 130, 177, 139, 141, 144, 171, 160, 127, 124,  26,  48, 215, 220, 249,
  119,  46, 125, 157, 173, 108,  95, 245,   0,  85, 101, 217,  74,  10, 142,  73,
  146, 231, 190,  53, 129, 115,  77, 140,  40, 104,  70,   2, 254, 106,  25, 205,
  175,  88, 136,  42,  81, 248, 151, 211,  24,  92, 246,   3, 165,   8,  68, 156,
   39,  50,  45, 147, 161, 233, 239, 250,  99, 201, 149, 184,  49, 243,  56,  62,
  196,   5, 234, 222, 123,  12, 208, 145, 112, 100,  97,  96, 223,  11, 174, 169,
  150, 176, 252, 204,  59, 154,  30,  55, 244,  57, 138,  93,  75,  67,  80, 105,
  188,  72, 183, 195,  47, 153,  23, 251,  17,  22,  32, 221, 118, 191,  69, 179,
  182, 126,  86,  83, 212, 200, 121, 255,   9, 206,  34, 240, 186, 194,  51, 135,
   98, 253, 180,  20, 128, 237, 224,  21,  60,  66, 219,  28,  27, 167, 213, 187,
  172, 203,  90, 152, 158, 185,  78, 230,  44,  15, 148, 214,  16, 133,  79,   7,
   52, 198, 189, 109, 197, 242, 192,  37, 181, 117, 236,  13, 227, 199, 232,  82};

static const uint8_t shortAbs(uint8_t n1, uint8_t n2) {
  uint8_t distance = n1 - n2;
  if (distance > 127) {
    return 255 - distance;
  } else {
    return distance;
  }
}

uint32_t mergeColors(uint32_t c1, uint32_t c2, uint8_t weight) {
  return (uint32_t)(((c1 >> 24) & 0xff) * weight / 255 + ((c2 >> 24) & 0xff) * (255 - weight) / 255) << 24 |
    (((c1 >> 16) & 0xff) * weight / 255 + ((c2 >> 16) & 0xff) * (255 - weight) / 255) << 16 |
    (((c1 >> 8) & 0xff) * weight / 255 + ((c2 >> 8) & 0xff) * (255 - weight) / 255) << 8 |
    ((c1 & 0xff) * weight / 255 + (c2 & 0xff) * (255 - weight) / 255);
}

uint32_t wheel(uint8_t position) {
  if(position < 85) {
   return Adafruit_NeoPixel::Color(position * 3, 255 - position * 3, 0);
  } else if(position < 170) {
   position -= 85;
   return Adafruit_NeoPixel::Color(255 - position * 3, 0, position * 3);
  } else {
   position -= 170;
   return Adafruit_NeoPixel::Color(0, position * 3, 255 - position * 3);
  }
}

void animation_off(Adafruit_NeoPixel &strip, uint32_t color1, uint32_t color2, uint8_t speed, uint8_t width, uint32_t tick) {
  strip.clear();
}

void animation_color(Adafruit_NeoPixel &strip, uint32_t color1, uint32_t color2, uint8_t speed, uint8_t width, uint32_t tick) {
  strip.fill(color1);
}

void animation_rainbow(Adafruit_NeoPixel &strip, uint32_t color1, uint32_t color2, uint8_t speed, uint8_t width, uint32_t tick) {
  for (uint16_t pixel = 0; pixel < strip.numPixels(); pixel++) {
    strip.setPixelColor(pixel, wheel(tick * speed / AVG_SPEED + (pixel * width / AVG_WIDTH)));
  }
}

void animation_twinkle(Adafruit_NeoPixel &strip, uint32_t baseColor, uint32_t twinkleColor, uint8_t speed, uint8_t width, uint32_t tick) {
  uint8_t distance;
  uint8_t position = tick * speed / 128;
  for (uint16_t pixel = 0; pixel < strip.numPixels(); pixel++) {
    distance = shortAbs(position, pgm_read_byte(&RandomTable[pixel % 256]));

    if (distance >= width) {
      strip.setPixelColor(pixel, baseColor);
    } else {
      strip.setPixelColor(pixel, mergeColors(baseColor, twinkleColor, (distance << 8) / width));
    }
  }
}

uint8_t ANIMATION_COUNT = 4;
Animation animations[] = {
  animation_off,
  animation_color,
  animation_rainbow,
  animation_twinkle
};

void updateStrip(Adafruit_NeoPixel &strip, AnimationConfig &config, uint32_t tick) {
  config.animation(strip, config.color1, config.color2, config.speed, config.width, tick);
}

void setup() {
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif

  for (int strip = 0; strip < STRIPS_COUNT; strip++) {
    strips[strip]->begin();
    strips[strip]->show();
    strips[strip]->setBrightness(60);

    animationConfigs[strip].animation = animation_color;
    animationConfigs[strip].color1 = Adafruit_NeoPixel::Color(0, 0, 0, 255);
  }

  Serial.begin(SERIAL_SPEED);
  Serial.write("Initialized\r\n");
}

void loop() {
  if (Serial.available()) {
    jsonDoc.clear();
    jsonError = deserializeJson(jsonDoc, Serial);

    if (jsonError) {
      Serial.print(F("Invalid JSON: "));
      Serial.println(jsonError.c_str());
    } else {
      Serial.println(F("Got new configuration"));
      JsonArray array = jsonDoc.as<JsonArray>();
      for (unsigned int index = 0; index < array.size() && index < STRIPS_COUNT; index++) {
        JsonObject json = array[index];
        if ((uint8_t)json["anim"] < ANIMATION_COUNT) {
        animationConfigs[index].animation = animations[(uint8_t)json["anim"]];
        }
        animationConfigs[index].color1 = (uint32_t)json["col1"];
        animationConfigs[index].color2 = (uint32_t)json["col2"];
        animationConfigs[index].speed = (uint8_t)json["speed"];
        animationConfigs[index].width = (uint8_t)json["width"];
      }
      startTick = millis();
    }
  }

  currentTick = millis() - startTick;
  for (int strip = 0; strip < STRIPS_COUNT; strip++) {
    if (animationConfigs[strip].animation) {
      updateStrip(*strips[strip], animationConfigs[strip], currentTick);
    }
  }
  //animation_rainbow(*strips[2], AVG_SPEED, AVG_WIDTH, currentTick);
  //animation_twinkle(*strips[2], Adafruit_NeoPixel::Color(255, 16, 0), Adafruit_NeoPixel::Color(255, 255, 128), AVG_SPEED, AVG_WIDTH, currentTick);
  //animation_twinkle(*strips[2], Adafruit_NeoPixel::Color(255, 128, 0), Adafruit_NeoPixel::Color(255, 255, 128), AVG_SPEED, AVG_WIDTH, currentTick);

  for (int strip = 0; strip < STRIPS_COUNT; strip++) {
    if (Serial.available()) return;
    strips[strip]->show();
  }
}