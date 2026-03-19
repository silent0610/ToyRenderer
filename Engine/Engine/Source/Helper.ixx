module;
export module Engine.Helper;
import std;
export namespace Engine::Helper
{

    std::vector<unsigned char> ConvertRgbToRgba(const unsigned char *rgb, int width, int height)
    {
        std::vector<unsigned char> rgba;
        rgba.resize(width * height * 4);

        for (int i = 0; i < width * height; ++i)
        {
            rgba[i * 4 + 0] = rgb[i * 3 + 0];
            rgba[i * 4 + 1] = rgb[i * 3 + 1];
            rgba[i * 4 + 2] = rgb[i * 3 + 2];
            rgba[i * 4 + 3] = 255; // Alpha channel set to opaque
        }

        return rgba;
    }
}