#include <iostream>
#include <string_view>
#include <format>
#include <vector>
#include <numbers>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

// Prototypes for optional outputs
void outputRoundTripImage(const std::string& filename);
void outputAngleStrengthEncoding(const std::string& filename);
void output2DVectorEncoding(const std::string& filename);

std::string_view usage()
{
    return R"(
Usage: anisotropinator.exe <inputfile>
    Simple utility created for us to evaluate encoding anisotropy texture data in 2 channels, 
    with xy representing a 2D vector and strength encoded as the magnitude of the vector.

Inputs:
    <inputfile> - An anisotropy texture encoded in 3 channels: x,y direction and anisotropy strength
Outputs:
    <inputfile>.transformed.png - anisotropy encoded in 2 channels, xy represents 
                                  a 2D vector with strength encoded as the magnitude
                                  of the vector.

    <inputfile>.roundtrip.png -   anisotropy file as an x,y direction and separate 
                                  strength channel; this is produced by loading the 
                                  transformed.png and encoding back into the source encoding.

    <inputfile>.angle.png -       anisotropy file encoded as 2 channels; an angle direction 
                                  and strength. This is produced by loading the transformed.png
                                  and encoding into this representation.
)";
}

void normalize(float& x, float& y)
{
    float magnitude = sqrt(x * x + y * y);
    if (magnitude > 0.f)
    {
        x /= magnitude;
        y /= magnitude;
    }
}

void toVecSpace(float& x, float& y)
{
    // map to [-1,1]
    x = (x / 255.f - 0.5f) * 2.f;
    y = (y / 255.f - 0.5f) * 2.f;
}

void toTexSpace(float& x, float& y)
{
    // map to [0,1]
    x = (x + 1.f) * 0.5f;
    y = (y + 1.f) * 0.5f;
    x = std::min(x, 1.f);
    y = std::min(y, 1.f);
}

float toDirectionAngle(float x, float y)
{
    // normalized 2d vector to angular rotation, [0, 2pi]
    float unit[2] = { 0.f, 1.f };
    float xydotunit = std::min(x * unit[0] + y * unit[1], 1.f);
    float theta = acos(xydotunit);
    if (x < 0.f)
    {
        theta = 2.f * std::numbers::pi_v<float> - theta;
    }

    return theta;
}

uint8_t angleToUNorm(float theta)
{
    float v = theta / (2.f * std::numbers::pi_v<float>);
    return std::clamp<uint8_t>(uint8_t(v * 255.f), 0, 255);
}

std::pair<float, float> bakeStrength(unsigned char x, unsigned char y, unsigned char strength)
{
    float dirx = float(x);
    float diry = float(y);
    toVecSpace(dirx, diry);

    normalize(dirx, diry);

    dirx *= (strength / 255.f);
    diry *= (strength / 255.f);

    toTexSpace(dirx, diry);

    return { dirx, diry };
}

std::string stripExt(const std::string& filename)
{
    size_t pos = filename.find_last_of('.');
    return filename.substr(0, pos);
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << usage();
        return 0;
    }

    char* filename = argv[1];

    output2DVectorEncoding(filename);
    outputRoundTripImage(filename);
    outputAngleStrengthEncoding(filename);

    return 0;
}

void output2DVectorEncoding(const std::string& filename)
{
    int w, h, n;
    unsigned char* data = stbi_load(filename.c_str(), &w, &h, &n, 0);

    std::vector<uint8_t> xformedData(w * h * 3);

    size_t srcOffset = 0;
    size_t destOffset = 0;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            unsigned char dirx = data[srcOffset];
            unsigned char diry = data[srcOffset + 1];
            unsigned char str = data[srcOffset + 2];
            srcOffset += n;

            // reduce from x,y direction + strength (3 channels) to
            // an x,y direction with a magnitude representing strength
            auto [fdirx, fdiry] = bakeStrength(dirx, diry, str);

            xformedData[destOffset] = uint8_t(fdirx * 255.f);
            xformedData[destOffset + 1] = uint8_t(fdiry * 255.f);
            xformedData[destOffset + 2] = 0;
            destOffset += 3;
        }
    }
    stbi_image_free(data);

    std::string xformedFile = std::format("{0}.transformed.png", stripExt(filename));
    int result = stbi_write_png(xformedFile.c_str(), w, h, 3, xformedData.data(), w * 3);
}

void outputRoundTripImage(const std::string& filename)
{
    std::string xformedFile = std::format("{0}.transformed.png", stripExt(filename));

    int w, h, n;
    uint8_t* data = stbi_load(xformedFile.c_str(), &w, &h, &n, 0);
    std::vector<uint8_t> roundTripData(w * h * 3);

    int srcOffset = 0;
    int destOffset = 0;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            float dirx = float(data[srcOffset]);
            float diry = float(data[srcOffset + 1]);
            srcOffset += 3;
            toVecSpace(dirx, diry);

            float strength = std::min(sqrt(dirx * dirx + diry * diry), 1.f);
            normalize(dirx, diry);

            toTexSpace(dirx, diry);


            roundTripData[destOffset] = uint8_t(dirx * 255.f);
            roundTripData[destOffset + 1] = uint8_t(diry * 255.f);
            roundTripData[destOffset + 2] = uint8_t(strength * 255.f);
            destOffset += 3;
        }
    }
    stbi_image_free(data);

    std::string roundTripFile = std::format("{0}.roundtrip.png", stripExt(filename));
    int result = stbi_write_png(roundTripFile.c_str(), w, h, 3, roundTripData.data(), w * 3);
}

void outputAngleStrengthEncoding(const std::string& filename)
{
    std::string xformedFile = std::format("{0}.transformed.png", stripExt(filename));

    int w, h, n;
    uint8_t* data = stbi_load(xformedFile.c_str(), &w, &h, &n, 0);
    std::vector<uint8_t> angleData(w * h * 3);

    int srcOffset = 0;
    int destOffset = 0;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            float dirx = float(data[srcOffset]);
            float diry = float(data[srcOffset + 1]);
            srcOffset += 3;
            toVecSpace(dirx, diry);

            float strength = std::min(sqrt(dirx * dirx + diry * diry), 1.f);
            normalize(dirx, diry);

            float theta = toDirectionAngle(dirx, diry);

            angleData[destOffset] = angleToUNorm(theta);
            angleData[destOffset + 1] = uint8_t(strength * 255.f);
            angleData[destOffset + 2] = 0;
            destOffset += 3;
        }
    }
    stbi_image_free(data);

    std::string angleFile = std::format("{0}.angle.png", stripExt(filename));
    int result = stbi_write_png(angleFile.c_str(), w, h, 3, angleData.data(), w * 3);
}

