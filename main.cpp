#include <iostream>
#include <string_view>
#include <format>
#include <vector>
#include <numbers>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include <unordered_map>

enum class Type
{
    eOld3Channel,
    e3Channel,
    e2D,
    eAngle
};
struct AnisotropyData
{
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    int numChannels = 0;
    Type type;
};

// Prototypes for optional outputs
AnisotropyData loadData(const std::string& filename, Type anisotropyType);
AnisotropyData old3_to_new3(const AnisotropyData& input);
AnisotropyData angle_to_new3(const AnisotropyData& input);
AnisotropyData mag2d_to_new3(const AnisotropyData& input);
AnisotropyData mag2d_to_angle(const AnisotropyData& input);
AnisotropyData new3_to_angle(const AnisotropyData& input);
AnisotropyData new3_to_mag2d(const AnisotropyData& input);
void writeData(const std::string& inputfilename, const AnisotropyData& transformed);

std::string_view usage()
{
    return R"(
Usage: anisotropinator.exe <inputfile> <inputtype> <outputtype>
    Simple utility created for us to evaluate encoding anisotropy texture data in 2 channels, 
    with xy representing a 2D vector and strength encoded as the magnitude of the vector.

Inputs:
    <inputfile> - An anisotropy texture encoded in 3 channels: x,y direction and anisotropy strength
    <inputtype> - Describes how anisotropy is encoded in the <inputfile>
                  3channel - anisotropy is encoded as a 2D direction and a strength [0-1]
                  3channel2 - anisotropy is encoded as a 2D direction and a strength [-1-1]
                  2D - anistropy is encoded as a 2D diretion, with the magnitude indicating the strength
                  angle     - anisotropy is encoded as an angular rotation [0-360] and a strength [0-1]
    <outputtype> - Desribes how anisotropy should be encoded in the <outputfile>. See <inputtype> for list of valid keywords.

Outputs:
    <inputfile>.[postfix].png
        3channel - anisotropy is encoded as a 2D direction and a strength [0-1]
        2D - anistropy is encoded as a 2D diretion, with the magnitude indicating the strength
        angle - anisotropy is encoded as an angular rotation [0-360] and a strength [0-1]
    <inputfile>.2D.png -    anisotropy encoded in 2 channels, xy represents 
                                  a 2D vector with strength encoded as the magnitude
                                  of the vector.

    <inputfile>.3channel.png -   anisotropy file as an x,y direction and separate 
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

void angleToDir(uint8_t angle, float& x, float& y)
{
    float unit[2] = { 0.f, 1.f };
    float twopi = 2.f * std::numbers::pi_v<float>;
    float theta = twopi *(float(angle) / 255.f);
    if (theta >= std::numbers::pi_v<float>)
    {
        theta = theta - twopi;
    }
    // anticlockwise to clockwise
    theta = -theta;

    x = unit[0] * cos(theta) - unit[1] * sin(theta);
    y = unit[0] * sin(theta) + unit[1] * cos(theta);
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
    if (argc != 4)
    {
        std::cout << usage();
        return 0;
    }

    std::string filename = argv[1];
    std::string inputtype = argv[2];
    std::string outputtype = argv[3];

    std::unordered_map<std::string, Type> typeMapping = {
        {"3channel2", Type::eOld3Channel},
        {"3channel", Type::e3Channel},
        {"2D", Type::e2D},
        {"angle", Type::eAngle}
    };

    if (typeMapping.find(inputtype) == typeMapping.end() || typeMapping.find(outputtype) == typeMapping.end())
    {
        std::cout << usage();
        return 0;
    }

    Type outtype = typeMapping[outputtype];

    AnisotropyData loaded = loadData(filename, typeMapping[inputtype]);
    AnisotropyData transformed;

    if (loaded.type == Type::eOld3Channel)
    {
        loaded = old3_to_new3(loaded);
    }
    else if (loaded.type == Type::eAngle)
    {
        loaded = angle_to_new3(loaded);
    }

    if (loaded.type == Type::e3Channel)
    {
        if (outtype == Type::e2D)
        {
            transformed = new3_to_mag2d(loaded);
        }
        else if (outtype == Type::eAngle)
        {
            transformed = new3_to_angle(loaded);
        }
        else if (outtype == Type::e3Channel)
        {
            transformed = loaded;
        }
    }

    if (loaded.type == Type::e2D)
    {
        if (outtype == Type::eAngle)
        {
            transformed = mag2d_to_angle(loaded);
        }
        else if (outtype == Type::e3Channel)
        {
            transformed = mag2d_to_new3(loaded);
        }
    }

    if (transformed.data.empty())
    {
        std::cout << "Unsupported conversion: " << inputtype << " to " << outputtype << std::endl;
        return 0;
    }

    writeData(filename, transformed);


    return 0;
}

AnisotropyData loadData(const std::string& filename, Type anisotropyType)
{
    //int numChannels = (anisotropyType == Type::e2D || anisotropyType == Type::eAngle) ? 2 : 3;
    int numChannels = 3;

    int w, h, n;
    unsigned char* input = stbi_load(filename.c_str(), &w, &h, &n, numChannels);

    assert(n == numChannels);
    std::vector<uint8_t> result(w * h * numChannels);
    memcpy(result.data(), input, result.size());

    stbi_image_free(input);

    return { .data = result, .width = w, .height = h, .numChannels = numChannels, .type = anisotropyType };
}

AnisotropyData old3_to_new3(const AnisotropyData& old3channel)
{
    AnisotropyData result;
    result.width = old3channel.width;
    result.height = old3channel.height;
    result.numChannels = 3;
    result.type = Type::e3Channel;

    result.data.resize(result.width * result.height * result.numChannels);

    size_t srcOffset = 0;
    size_t destOffset = 0;
    for (int y = 0; y < old3channel.height; ++y)
    {
        for (int x = 0; x < old3channel.width; ++x)
        {
            uint8_t dirx = old3channel.data[srcOffset];
            uint8_t diry = old3channel.data[srcOffset + 1];
            uint8_t str = old3channel.data[srcOffset + 2];
            srcOffset += old3channel.numChannels;

            if (str < 128)
            {
                std::swap(dirx, diry);
                str = 128 - str;
            }
            else
            {
                str = str - 128;
            }
            str = uint8_t(255.0 * str / 128.0);

            result.data[destOffset] = dirx;
            result.data[destOffset + 1] = diry;
            result.data[destOffset + 2] = str;
            destOffset += result.numChannels;
        }
    }

    return result;
}

AnisotropyData angle_to_new3(const AnisotropyData& input)
{
    AnisotropyData result;
    result.width = input.width;
    result.height = input.height;
    result.numChannels = 3;
    result.type = Type::e2D;

    result.data.resize(result.width * result.height * result.numChannels);

    size_t srcOffset = 0;
    size_t destOffset = 0;
    for (int y = 0; y < input.height; ++y)
    {
        for (int x = 0; x < input.width; ++x)
        {
            uint8_t angle = input.data[srcOffset];
            uint8_t str = input.data[srcOffset + 1];
            srcOffset += input.numChannels;

            float dirx, diry;
            angleToDir(angle, dirx, diry);

            normalize(dirx, diry);

            result.data[destOffset] = uint8_t(dirx * 255.f);
            result.data[destOffset + 1] = uint8_t(diry * 255.f);
            result.data[destOffset + 2] = str;
        }
    }

    return result;
}

AnisotropyData mag2d_to_new3(const AnisotropyData& input)
{
    AnisotropyData result;
    result.width = input.width;
    result.height = input.height;
    result.numChannels = 3;
    result.type = Type::e3Channel;

    result.data.resize(result.width * result.height * result.numChannels);

    size_t srcOffset = 0;
    size_t destOffset = 0;
    for (int y = 0; y < input.height; ++y)
    {
        for (int x = 0; x < input.width; ++x)
        {
            float dirx = float(input.data[srcOffset]);
            float diry = float(input.data[srcOffset + 1]);
            srcOffset += input.numChannels;

            toVecSpace(dirx, diry);

            float strength = std::min(sqrt(dirx * dirx + diry * diry), 1.f);
            normalize(dirx, diry);

            toTexSpace(dirx, diry);


            result.data[destOffset] = uint8_t(dirx * 255.f);
            result.data[destOffset + 1] = uint8_t(diry * 255.f);
            result.data[destOffset + 2] = uint8_t(strength * 255.f);
            destOffset += 3;
        }
    }

    return result;
}

AnisotropyData mag2d_to_angle(const AnisotropyData& input)
{
    AnisotropyData result;
    result.width = input.width;
    result.height = input.height;
    result.numChannels = 3;
    result.type = Type::eAngle;

    result.data.resize(result.width * result.height * result.numChannels);

    size_t srcOffset = 0;
    size_t destOffset = 0;
    for (int y = 0; y < input.height; ++y)
    {
        for (int x = 0; x < input.width; ++x)
        {
            float dirx = float(input.data[srcOffset]);
            float diry = float(input.data[srcOffset + 1]);
            srcOffset += input.numChannels;
            toVecSpace(dirx, diry);

            float strength = std::min(sqrt(dirx * dirx + diry * diry), 1.f);
            normalize(dirx, diry);

            float theta = toDirectionAngle(dirx, diry);

            result.data[destOffset] = angleToUNorm(theta);
            result.data[destOffset + 1] = uint8_t(strength * 255.f);
            result.data[destOffset + 2] = 0;
            destOffset += 3;
        }
    }

    return result;
}

AnisotropyData new3_to_angle(const AnisotropyData& input)
{
    AnisotropyData result;
    result.width = input.width;
    result.height = input.height;
    result.numChannels = 3;
    result.type = Type::eAngle;

    result.data.resize(result.width * result.height * result.numChannels);

    size_t srcOffset = 0;
    size_t destOffset = 0;
    for (int y = 0; y < input.height; ++y)
    {
        for (int x = 0; x < input.width; ++x)
        {
            float dirx = float(input.data[srcOffset]);
            float diry = float(input.data[srcOffset + 1]);
            unsigned char str = input.data[srcOffset + 2];
            srcOffset += input.numChannels;
            toVecSpace(dirx, diry);
            normalize(dirx, diry);

            float theta = toDirectionAngle(dirx, diry);

            result.data[destOffset] = angleToUNorm(theta);
            result.data[destOffset + 1] = str;
            result.data[destOffset + 2] = 0;
            destOffset += 3;
        }
    }

    return result;
}

AnisotropyData new3_to_mag2d(const AnisotropyData& input)
{
    AnisotropyData result;
    result.width = input.width;
    result.height = input.height;
    result.numChannels = 3;
    result.type = Type::e2D;

    result.data.resize(result.width * result.height * result.numChannels);

    size_t srcOffset = 0;
    size_t destOffset = 0;
    for (int y = 0; y < input.height; ++y)
    {
        for (int x = 0; x < input.width; ++x)
        {
            unsigned char dirx = input.data[srcOffset];
            unsigned char diry = input.data[srcOffset + 1];
            unsigned char str = input.data[srcOffset + 2];
            srcOffset += input.numChannels;

            // reduce from x,y direction + strength (3 channels) to
            // an x,y direction with a magnitude representing strength
            auto [fdirx, fdiry] = bakeStrength(dirx, diry, str);

            result.data[destOffset] = uint8_t(fdirx * 255.f);
            result.data[destOffset + 1] = uint8_t(fdiry * 255.f);
            result.data[destOffset + 2] = 0;
            destOffset += 3;
        }
    }

    return result;
}

void writeData(const std::string& inputfilename, const AnisotropyData& transformed)
{
    std::unordered_map<Type, std::string> typeMapping = {
        { Type::eOld3Channel, "3channel2" },
        { Type::e3Channel, "3channel" },
        { Type::e2D, "2D" },
        { Type::eAngle, "angle" }
    };

    std::string outputfilename = std::format("{0}.{1}.png", stripExt(inputfilename), typeMapping[transformed.type]);
    int result = stbi_write_png(outputfilename.c_str(), transformed.width, transformed.height, transformed.numChannels, transformed.data.data(), transformed.width * transformed.numChannels);
}



