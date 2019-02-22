#include "Color.h"

Color::Color(int r, int g, int b, int f, int i) : R(r), G(g), B(b), Frame(f), Intensity(i) {}

void Color::ReplaceColor(const Color& color) {
    this->R = color.R;
    this->G = color.G;
    this->B = color.B;
    this->Intensity = color.Intensity;
}

void Color::ChangeColor(const int r, const int g, const int b, const int i) {
    this->R = r;
    this->G = g;
    this->B = b;

    if(i != -1)
        this->Intensity = i;
}

void Color::RandomizeColor(void) {
    int whichTop = ESP8266TrueRandom.random(3);

    int whichBot = -1;
    do {
        whichBot = ESP8266TrueRandom.random(3);
    } while (whichTop == whichBot);

    this->R = whichTop == 0 ? 255 : ESP8266TrueRandom.random(whichBot == 0 ? 50 : 256);
    this->G = whichTop == 1 ? 255 : ESP8266TrueRandom.random(whichBot == 1 ? 50 : 256);
    this->B = whichTop == 2 ? 255 : ESP8266TrueRandom.random(whichBot == 2 ? 50 : 256);
}
