#ifndef Color_h
#define Color_h

#include <ESP8266TrueRandom.h>

class Color {
    public:
        Color(int = 0, int = 0, int = 0, int = 0, int = 100);
        void ReplaceColor(const Color&);
        void ChangeColor(const int, const int, const int, const int = -1);
        void RandomizeColor(void);

        int R;
        int G;
        int B;
        int Frame;
        int Intensity;
};

#endif
