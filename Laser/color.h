#pragma once

class Color {
    float r,g,b;
 public:
    // New color with give RGB (0.0 - 1.0)
    Color(float _r, float _g, float _b) { r=_r; g=_g; b=_b; }
    float red() const { return r; }
    float green() const { return g; }
    float blue() const { return b; }
    bool operator==(const Color &c) const { return r==c.r && b==c.b && g==c.g; }
    friend std::ostream& operator<<(std::ostream& s, const Color &col);
    static Color getBasicColor(int i);   // Get color #i for graphic distinct colors
};

