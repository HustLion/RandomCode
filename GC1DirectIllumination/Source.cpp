#define _CRT_SECURE_NO_WARNINGS
 
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <array>
#include <thread>
#include <atomic>

#include "SVector.h"
#include "SSphere.h"
#include "SPlane.h"
#include "STriangle.h"
#include "SMaterial.h"
#include "SImageData.h"
#include "Utils.h"
#include "STimer.h"
#include "Random.h"

//=================================================================================
//                                 SETTINGS
//=================================================================================

// image size
static const size_t c_imageWidth = 512;
static const size_t c_imageHeight = 512;

// preview update rate
static const size_t c_redrawFPS = 30;

// sampling
static const bool c_jitterSamples = true;
static const size_t c_maxBounces = 5;
static const size_t c_russianRouletteStartBounce = 5;
static const size_t c_stratifyPixel = 8;

// camera - assumes no roll, and that (0,1,0) is up
static const SVector c_cameraPos = { -3.0f, 3.0f, -10.0f };
static const SVector c_cameraAt = { 0.0f, -1.0f, 0.0f };
static const float c_nearDist = 0.1f;
static const float c_cameraVerticalFOV = 60.0f * c_pi / 180.0f;

// Materials - name, diffuse, emissive, reflective, refractive, refractionIndex, brdf
#define MATERIALLIST() \
    MATERIAL(Black          , SVector(), SVector(), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(MatteRed       , SVector(0.9f, 0.1f, 0.1f), SVector(), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(MatteGreen     , SVector(0.1f, 0.9f, 0.1f), SVector(), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(MatteBlue      , SVector(0.1f, 0.1f, 0.9f), SVector(), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(MatteTeal      , SVector(0.1f, 0.9f, 0.9f), SVector(), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(MatteMagenta   , SVector(0.9f, 0.1f, 0.9f), SVector(), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(MatteYellow    , SVector(0.9f, 0.9f, 0.1f), SVector(), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(MatteWhite     , SVector(0.9f, 0.9f, 0.9f), SVector(), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(EmissiveRed    , SVector(), SVector(10.0f,  0.0f,  0.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(EmissiveGreen  , SVector(), SVector( 0.0f, 10.0f,  0.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(EmissiveBlue   , SVector(), SVector( 0.0f,  0.0f, 10.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(EmissiveTeal   , SVector(), SVector( 0.0f, 10.0f, 10.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(EmissiveMagenta, SVector(), SVector(10.0f,  0.0f, 10.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(EmissiveYellow , SVector(), SVector(10.0f, 10.0f,  0.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(EmissiveWhite  , SVector(), SVector(10.0f, 10.0f, 10.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(ShinyRed       , SVector(0.9f, 0.1f, 0.1f), SVector(), SVector(0.2f, 0.2f, 0.2f), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(ShinyGreen     , SVector(0.1f, 0.9f, 0.1f), SVector(), SVector(0.2f, 0.2f, 0.2f), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(ShinyBlue      , SVector(0.1f, 0.1f, 0.9f), SVector(), SVector(0.2f, 0.2f, 0.2f), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(ShinyTeal      , SVector(0.1f, 0.9f, 0.9f), SVector(), SVector(0.2f, 0.2f, 0.2f), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(ShinyMagenta   , SVector(0.9f, 0.1f, 0.9f), SVector(), SVector(0.2f, 0.2f, 0.2f), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(ShinyYellow    , SVector(0.9f, 0.9f, 0.1f), SVector(), SVector(0.2f, 0.2f, 0.2f), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(ShinyWhite     , SVector(0.9f, 0.9f, 0.9f), SVector(), SVector(0.2f, 0.2f, 0.2f), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(ShinyGrey      , SVector(0.1f, 0.1f, 0.1f), SVector(), SVector(0.2f, 0.2f, 0.2f), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(GlowRed        , SVector(0.1f, 0.1f, 0.1f), SVector(0.01f, 0.0f, 0.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(GlowGreen      , SVector(0.1f, 0.1f, 0.1f), SVector(0.0f, 0.2f, 0.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(GlowBlue       , SVector(0.1f, 0.1f, 0.1f), SVector(0.0f, 0.0f, 2.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(GlowTeal       , SVector(0.1f, 0.1f, 0.1f), SVector(0.0f, 2.0f, 2.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(GlowMagenta    , SVector(0.1f, 0.1f, 0.1f), SVector(2.0f, 0.0f, 2.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(GlowYellow     , SVector(0.1f, 0.1f, 0.1f), SVector(2.0f, 2.0f, 0.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(GlowWhite      , SVector(0.1f, 0.1f, 0.1f), SVector(2.0f, 2.0f, 2.0f), SVector(), SVector(), 1.0f, EBRDF::standard) \
    MATERIAL(Water          , SVector(), SVector(), SVector(0.1f, 0.1f, 0.1f), SVector(1.0f, 1.0f, 1.0f), 1.3f, EBRDF::standard) \
    MATERIAL(Chrome         , SVector(0.01f, 0.01f, 0.01f), SVector(), SVector(1.0f, 1.0f, 1.0f), SVector(), 1.0f, EBRDF::standard) \

#include "MakeMaterials.h"

// Spheres
auto c_spheres = make_array(
    SSphere(SVector(-2.0f, -3.0f,  4.0f), 2.0f, TMaterialID::ShinyTeal),
    SSphere(SVector( 0.3f, -3.5f,  0.5f), 1.5f, TMaterialID::ShinyMagenta),
    SSphere(SVector( 2.0f, -4.0f, -2.0f), 1.0f, TMaterialID::ShinyYellow),
    SSphere(SVector(-3.5f, -2.5f, -2.0f), 1.0f, TMaterialID::Water),
    SSphere(SVector(-3.0f,  5.0f, -3.0f), 0.5f, TMaterialID::EmissiveWhite),
    SSphere(SVector( 3.0f,  5.0f, -3.0f), 0.5f, TMaterialID::EmissiveRed),
    SSphere(SVector( 3.0f,  5.0f,  3.0f), 0.5f, TMaterialID::EmissiveBlue),
    SSphere(SVector(-3.0f,  5.0f,  3.0f), 0.5f, TMaterialID::EmissiveGreen)
);

const float c_boxSize = 5.0f;

// Planes
auto c_planes = make_array(
    // box walls : front, back, left, right, bottom, top
    SPlane(SVector( 0.0,  0.0, -1.0), c_boxSize, TMaterialID::MatteBlue),
    SPlane(SVector( 0.0,  0.0,  1.0),     11.0f, TMaterialID::Black),
    SPlane(SVector( 1.0,  0.0,  0.0), c_boxSize, TMaterialID::MatteRed),
    SPlane(SVector(-1.0,  0.0,  0.0), c_boxSize, TMaterialID::MatteGreen),
    SPlane(SVector( 0.0,  1.0,  0.0), c_boxSize, TMaterialID::ShinyYellow),
    SPlane(SVector( 0.0, -1.0,  0.0), c_boxSize, TMaterialID::MatteTeal)
);

// Triangles
auto c_triangles = make_array(
    STriangle(SVector(1.5f, 1.0f, 1.25f), SVector(1.5f, 0.0f, 1.75f), SVector(0.5f, 0.0f, 2.25f), TMaterialID::MatteYellow)
);

//=================================================================================
static SVector CameraRight ()
{
    SVector cameraFwd = c_cameraAt - c_cameraPos;
    Normalize(cameraFwd);

    SVector cameraRight = Cross(SVector(0.0f, 1.0f, 0.0f), cameraFwd);
    Normalize(cameraRight);

    SVector cameraUp = Cross(cameraFwd, cameraRight);
    Normalize(cameraUp);

    return cameraRight;
}

//=================================================================================
static SVector CameraUp ()
{
    SVector cameraFwd = c_cameraAt - c_cameraPos;
    Normalize(cameraFwd);

    SVector cameraRight = Cross(SVector(0.0f, 1.0f, 0.0f), cameraFwd);
    Normalize(cameraRight);

    SVector cameraUp = Cross(cameraFwd, cameraRight);
    Normalize(cameraUp);

    return cameraUp;
}

//=================================================================================
static SVector CameraFwd ()
{
    SVector cameraFwd = c_cameraAt - c_cameraPos;
    Normalize(cameraFwd);

    SVector cameraRight = Cross(SVector(0.0f, 1.0f, 0.0f), cameraFwd);
    Normalize(cameraRight);

    SVector cameraUp = Cross(cameraFwd, cameraRight);
    Normalize(cameraUp);

    return cameraFwd;
}

//=================================================================================
static const size_t c_numPixels = c_imageWidth * c_imageHeight;
static const float c_aspectRatio = float(c_imageWidth) / float(c_imageHeight);
static const float c_cameraHorizFOV = c_cameraVerticalFOV * c_aspectRatio;
static const float c_windowTop = tan(c_cameraVerticalFOV / 2.0f) * c_nearDist;
static const float c_windowRight = tan(c_cameraHorizFOV / 2.0f) * c_nearDist;
static const SVector c_cameraRight = CameraRight();
static const SVector c_cameraUp = CameraUp();
static const SVector c_cameraFwd = CameraFwd();
static const size_t c_RRBounceLeftBegin = c_maxBounces > c_russianRouletteStartBounce ? c_maxBounces - c_russianRouletteStartBounce : 0;

static const size_t c_numThreads = std::thread::hardware_concurrency();
static size_t g_numThreadsActive = c_numThreads;

std::atomic<bool> g_wantsExit(false);

static std::atomic<size_t> g_currentPixelIndex(-1);

// make an RGB f32 texture.
// lower left of image is (0,0).
static SImageDataRGBF32 g_image_RGB_F32(c_imageWidth, c_imageHeight);

//=================================================================================
bool AnyIntersection (const SVector& a, const SVector& dir, float length, TObjectID ignoreObjectID = c_invalidObjectID)
{
    SCollisionInfo collisionInfo;
    collisionInfo.m_maxCollisionTime = length;
    for (const SSphere& s : c_spheres)
    {
        if (RayIntersects(a, dir, s, collisionInfo, ignoreObjectID))
            return true;
    }
    for (const SPlane& p : c_planes)
    {
        if (RayIntersects(a, dir, p, collisionInfo, ignoreObjectID))
            return true;
    }
    for (const STriangle& t : c_triangles)
    {
        if (RayIntersects(a, dir, t, collisionInfo, ignoreObjectID))
            return true;
    }

    return false;
}

//=================================================================================
bool ClosestIntersection (const SVector& rayPos, const SVector& rayDir, SCollisionInfo& collisionInfo, TObjectID ignoreObjectID = c_invalidObjectID)
{
    bool ret = false;
    for (const SSphere& s : c_spheres)
        ret |= RayIntersects(rayPos, rayDir, s, collisionInfo, ignoreObjectID);
    for (const SPlane& p: c_planes)
        ret |= RayIntersects(rayPos, rayDir, p, collisionInfo, ignoreObjectID);
    for (const STriangle& t : c_triangles)
        ret |= RayIntersects(rayPos, rayDir, t, collisionInfo, ignoreObjectID);
    return ret;
}

//=================================================================================
inline bool PassesRussianRoulette (const SVector& v, size_t bouncesLeft)
{
    // if the vector is 0, it fails Russian roulette test always
    if (!NotZero(v))
        return false;

    // otherwise, only test if we are past the point of when Russian roulette should start
    if (bouncesLeft >= c_RRBounceLeftBegin)
        return true;

    // else leave it to chance based on the magnitude of the largest component of the vector
    return RandomFloat() <= MaxComponentValue(v);
}

//=================================================================================
SVector L_out(const SCollisionInfo& X, const SVector& outDir, size_t bouncesLeft)
{
    // if no bounces left, return black / darkness
    if (bouncesLeft == 0)
        return SVector();

    const SMaterial& material = c_materials[(size_t)X.m_materialID];

    // start with emissive lighting
    SVector ret = material.m_emissive;

    // Add in BRDF pulses.
    // Pulses are strong sampling points which on the BRDF which would be basically impossible to hit with monte carlo.

    // add in reflection BRDF pulse.  
    if (PassesRussianRoulette(material.m_reflection, bouncesLeft))
    {
        SVector reflectDir = Reflect(-outDir, X.m_surfaceNormal);
        SCollisionInfo collisionInfo;
        if (ClosestIntersection(X.m_intersectionPoint, reflectDir, collisionInfo, X.m_objectID))
        {
            float cos_theta = Dot(reflectDir, X.m_surfaceNormal);
            SVector BRDF = 2.0f * material.m_reflection * cos_theta;
            ret += BRDF * L_out(collisionInfo, -reflectDir, bouncesLeft - 1);
        }
    }

    // add in refraction BRDF pulse.
    if (PassesRussianRoulette(material.m_refraction, bouncesLeft))
    {
        // make our refraction index ratio.
        // air has a refractive index of just over 1.0, and vacum has 1.0.
        float ratio = X.m_fromInside ? material.m_refractionIndex / 1.0f : 1.0f / material.m_refractionIndex;
        SVector refractDir = Refract(-outDir, X.m_surfaceNormal, ratio);
        SCollisionInfo collisionInfo;

        // We need to push the ray out a little bit, instead of telling it to ignore this object for the intersection
        // test, because we may hit the same object again legitimately!
        if (ClosestIntersection(X.m_intersectionPoint + refractDir * 0.001f, refractDir, collisionInfo))
        {
            float cos_theta = Dot(refractDir, X.m_surfaceNormal);
            SVector BRDF = 2.0f * material.m_refraction * cos_theta;
            ret += BRDF * L_out(collisionInfo, -refractDir, bouncesLeft - 1);
        }
    }

    // add in random samples for global illumination etc
    if (PassesRussianRoulette(material.m_diffuse, bouncesLeft))
    {
        SVector newRayDir = UniformSampleHemisphere(X.m_surfaceNormal);
        SCollisionInfo collisionInfo;
        if (ClosestIntersection(X.m_intersectionPoint, newRayDir, collisionInfo, X.m_objectID))
        {
            float cos_theta = Dot(newRayDir, X.m_surfaceNormal);
            SVector BRDF = 2.0f * material.m_diffuse * cos_theta;
            ret += BRDF * L_out(collisionInfo, -newRayDir, bouncesLeft - 1);
        }
    }
    return ret;
}

//=================================================================================
SVector L_in (const SVector& X, const SVector& dir)
{
    // if this ray doesn't hit anything, return black / darkness
    SCollisionInfo collisionInfo;
    if (!ClosestIntersection(X, dir, collisionInfo))
        return SVector();

    // else, return the amount of light coming towards us from the object we hit
    return L_out(collisionInfo, -dir, c_maxBounces);
}

//=================================================================================
void RenderPixel (float u, float v, SVector& pixel)
{
    // make (u,v) go from [-1,1] instead of [0,1]
    u = u * 2.0f - 1.0f;
    v = v * 2.0f - 1.0f;

    // find where the ray hits the near plane, and normalize that vector to get the ray direction.
    SVector rayStart = c_cameraPos + c_cameraFwd * c_nearDist;
    rayStart += c_cameraRight * c_windowRight * u;
    rayStart += c_cameraUp * c_windowTop * v;
    SVector rayDir = rayStart - c_cameraPos;
    Normalize(rayDir);

    // our pixel is the amount of light coming in to the position on our near plane from the ray direction.
    pixel = L_in(rayStart, rayDir);
}

//=================================================================================
void ThreadFunc()
{
    static std::atomic<size_t> nextThreadId(-1);
    size_t threadId = ++nextThreadId;

    // TODO: g_currentPixelIndex should get a row of pixels at a time instead of single pixel, i think

    // render individual pixels across multiple threads until we run out of pixels to do
    size_t pixelIndex = ++g_currentPixelIndex;
    bool firstThread = pixelIndex == 0;
    while (!g_wantsExit.load())
    {
        size_t sampleCount = 1 + pixelIndex / g_image_RGB_F32.m_pixels.size();

        // get the current pixel's UV and actual memory location
        size_t x = (pixelIndex % g_image_RGB_F32.m_pixels.size()) % g_image_RGB_F32.m_width;
        size_t y = (pixelIndex % g_image_RGB_F32.m_pixels.size()) / g_image_RGB_F32.m_height;
        float xPercent = (float)x / (float)g_image_RGB_F32.m_width;
        float yPercent = (float)y / (float)g_image_RGB_F32.m_height;
        RGB_F32& pixel = g_image_RGB_F32.m_pixels[pixelIndex % g_image_RGB_F32.m_pixels.size()];

        // calculate +/- half a pixel jitter, for anti aliasing
        float jitterX = 0.0f;
        float jitterY = 0.0f;
        if (false)//c_jitterSamples)
        {
            // stratify the jittering to be within a specific cell of a grid
            int index = ((pixelIndex * 101) % (c_stratifyPixel*c_stratifyPixel));
            int indexX = index % c_stratifyPixel;
            int indexY = index / c_stratifyPixel;

            float minX = float(indexX / float(c_stratifyPixel));
            float maxX = minX + 1.0f / float(c_stratifyPixel);
            float minY = float(indexY / float(c_stratifyPixel));
            float maxY = minY + 1.0f / float(c_stratifyPixel);

            jitterX = (RandomFloat()-0.5f) / (float)g_image_RGB_F32.m_width;
            jitterY = (RandomFloat()-0.5f) / (float)g_image_RGB_F32.m_height;
        }

        // render a pixel sample
        SVector out;
        RenderPixel(xPercent + jitterX, yPercent + jitterY, out);

        // add the sample to this pixel using incremental averaging
        pixel[0] += (out.m_x - pixel[0]) / float(sampleCount);
        pixel[1] += (out.m_y - pixel[1]) / float(sampleCount);
        pixel[2] += (out.m_z - pixel[2]) / float(sampleCount);

        // sleep this thread if we are supposed to
        while (threadId >= g_numThreadsActive && !g_wantsExit.load())
            Sleep(100);

        // move to next pixel
        pixelIndex = ++g_currentPixelIndex;
    }
}


//=================================================================================
void CaptureImage (SImageDataBGRU8& dest, const SImageDataRGBF32& src)
{
    for (size_t y = 0; y < c_imageHeight; ++y)
    {
        const RGB_F32 *srcPixel = &src.m_pixels[y*src.m_width];
        BGR_U8 *destPixel = (BGR_U8*)&dest.m_pixels[y*dest.m_pitch];
        for (size_t x = 0; x < c_imageWidth; ++x)
        {
            // apply SRGB correction
            RGB_F32 correctedPixel;
            correctedPixel[0] = powf((*srcPixel)[0], 1.0f / 2.2f);
            correctedPixel[1] = powf((*srcPixel)[1], 1.0f / 2.2f);
            correctedPixel[2] = powf((*srcPixel)[2], 1.0f / 2.2f);

            // convert from float to uint8
            (*destPixel)[0] = uint8(Clamp(correctedPixel[2] * 255.0f, 0.0f, 255.0f));
            (*destPixel)[1] = uint8(Clamp(correctedPixel[1] * 255.0f, 0.0f, 255.0f));
            (*destPixel)[2] = uint8(Clamp(correctedPixel[0] * 255.0f, 0.0f, 255.0f));
            ++srcPixel;
            ++destPixel;
        }
    }
}

//=================================================================================
HBITMAP CaptureImageAsBitmap (const SImageDataRGBF32& src)
{
    HDC dc = GetDC(nullptr);

    BITMAPINFO header;
    header.bmiHeader.biSize = sizeof(header);
    header.bmiHeader.biWidth = (LONG)src.m_width;
    header.bmiHeader.biHeight = (LONG)src.m_height;
    header.bmiHeader.biPlanes = 1;
    header.bmiHeader.biBitCount = 32;
    header.bmiHeader.biCompression = BI_RGB;
    header.bmiHeader.biSizeImage = 0;
    header.bmiHeader.biXPelsPerMeter = 0;
    header.bmiHeader.biYPelsPerMeter = 0;
    header.bmiHeader.biClrUsed = 0;
    header.bmiHeader.biClrImportant = 0;

    uint8 *destPixels = nullptr;
    HBITMAP hbmp = CreateDIBSection(dc, &header, DIB_RGB_COLORS, (void**)&destPixels, NULL, 0);
    if (hbmp)
    {
        for (size_t y = 0; y < c_imageHeight; ++y)
        {
            const RGB_F32 *srcPixel = &src.m_pixels[y*src.m_width];
            uint8 *destPixel = &destPixels[y*src.m_width*4];
            for (size_t x = 0; x < c_imageWidth; ++x)
            {
                // apply SRGB correction
                RGB_F32 correctedPixel;
                correctedPixel[0] = powf((*srcPixel)[0], 1.0f / 2.2f);
                correctedPixel[1] = powf((*srcPixel)[1], 1.0f / 2.2f);
                correctedPixel[2] = powf((*srcPixel)[2], 1.0f / 2.2f);

                // convert to uint8
                destPixel[0] = uint8(Clamp(correctedPixel[2] * 255.0f, 0.0f, 255.0f));
                destPixel[1] = uint8(Clamp(correctedPixel[1] * 255.0f, 0.0f, 255.0f));
                destPixel[2] = uint8(Clamp(correctedPixel[0] * 255.0f, 0.0f, 255.0f));
                destPixel[3] = 255;
                ++srcPixel;
                destPixel += 4;;
            }
        }
    }

    ReleaseDC(nullptr, dc);
    return hbmp;
}

//=================================================================================
void TakeScreenshot ()
{
    // Convert from RGB floating point to BGR u8
    SImageDataBGRU8 image_BGR_U8(c_imageWidth, c_imageHeight);
    CaptureImage(image_BGR_U8, g_image_RGB_F32);

    // find a unique filename
    int index = 1;
    char filename[1024];
    strcpy(filename, "screenshot.bmp");
    FILE *file = fopen(filename, "rb");
    while (file)
    {
        fclose(file);
        sprintf(filename, "screenshot%i.bmp", index);
        ++index;
        file = fopen(filename, "rb");
    }

    // save the screenshot
    SaveImage(filename, image_BGR_U8);
}

//=================================================================================
LRESULT __stdcall WindowProcedure (HWND window, unsigned int msg, WPARAM wp, LPARAM lp)
{
    static HBITMAP hbitmap = nullptr;
    static STimer timer;

    // handle the message
    switch (msg)
    {
        case WM_KEYDOWN:
        {
            switch (wp)
            {
                case VK_DOWN: g_numThreadsActive = g_numThreadsActive > 0 ? --g_numThreadsActive : 0; return 0;
                case VK_UP: g_numThreadsActive = g_numThreadsActive < c_numThreads ? ++g_numThreadsActive : c_numThreads; return 0;
                case VK_ESCAPE: DeleteObject(hbitmap); g_wantsExit = true; return 0;
                case 'S': TakeScreenshot(); return 0;
                default: return DefWindowProc(window, msg, wp, lp);
            }
        }
        case WM_TIMER:
        {            
            // calculate our time
            size_t secondsElapsed = timer.ElapsedSeconds();
            size_t secondsTotal = secondsElapsed;
            size_t hours = secondsTotal / (60 * 60);
            secondsTotal = secondsTotal % (60 * 60);
            size_t minutes = secondsTotal / 60;
            secondsTotal = secondsTotal % 60;
            size_t seconds = secondsTotal;

            // calculate our sample count
            size_t sampleCount = g_currentPixelIndex.load() / g_image_RGB_F32.m_pixels.size();

            // calculate samples per second
            float samplesPerSecond = float(sampleCount) / float(secondsElapsed);

            // update the window title
            char buffer[1024];
            sprintf(buffer, "%i samples, %i:%s%i:%s%i, %0.0f samples per second, %i/%i threads, %ix%i", sampleCount, hours, minutes < 10 ? "0" : "", minutes, seconds < 10 ? "0" : "", seconds, samplesPerSecond, g_numThreadsActive, c_numThreads, c_imageWidth, c_imageHeight);
            SetWindowTextA(window, buffer);

            // delete the old bitmap
            DeleteObject(hbitmap);

            // capture the current image
            hbitmap = CaptureImageAsBitmap(g_image_RGB_F32);

            // redraw
            InvalidateRect(window, nullptr, false);
            return DefWindowProc(window, msg, wp, lp);
        }
        case WM_PAINT:
        {
            // draw bitmap
            PAINTSTRUCT 	ps;
            HDC 			hdc;
            BITMAP 			bitmap;
            HDC 			hdcMem;
            HGDIOBJ 		oldBitmap;

            hdc = BeginPaint(window, &ps);

            hdcMem = CreateCompatibleDC(hdc);
            oldBitmap = SelectObject(hdcMem, hbitmap);

            GetObject(hbitmap, sizeof(bitmap), &bitmap);
            BitBlt(hdc, 0, 0, bitmap.bmWidth, bitmap.bmHeight, hdcMem, 0, 0, SRCCOPY);

            SelectObject(hdcMem, oldBitmap);
            DeleteDC(hdcMem);

            EndPaint(window, &ps);

            return 0;
        }
        case WM_MBUTTONDOWN:
        {
            return DefWindowProc(window, msg, wp, lp);
        }
        case WM_CLOSE:
        {
            DeleteObject(hbitmap);
            g_wantsExit = true;
            return 0;
        }
        default:
        {
            return DefWindowProc(window, msg, wp, lp);
        }
    }
}

//=================================================================================
void WindowFunc ()
{
    WNDCLASSEX wndclass = { sizeof(WNDCLASSEX), CS_DBLCLKS, WindowProcedure,
                            0, 0, GetModuleHandle(0), LoadIcon(0,IDI_APPLICATION),
                            LoadCursor(0,IDC_ARROW), HBRUSH(COLOR_WINDOW+1),
                            0, L"myclass", LoadIcon(0,IDI_APPLICATION) } ;
    if( RegisterClassEx(&wndclass) )
    {
        HWND window = CreateWindowEx( 0, L"myclass", L"title",
            (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU), CW_USEDEFAULT, CW_USEDEFAULT,
                    c_imageWidth, c_imageHeight, 0, 0, GetModuleHandle(0), 0 ) ;
        if(window)
        {
            // start a timer, for us to get the latest rendered image
            SetTimer(window, 0, 1000/c_redrawFPS, nullptr);

            ShowWindow( window, SW_SHOWDEFAULT ) ;
            MSG msg ;
            while( GetMessage( &msg, 0, 0, 0 ) && !g_wantsExit.load())
                DispatchMessage(&msg) ;
        }
    }
}

//=================================================================================
int CALLBACK WinMain(
  _In_ HINSTANCE hInstance,
  _In_ HINSTANCE hPrevInstance,
  _In_ LPSTR     lpCmdLine,
  _In_ int       nCmdShow
)
{
    // spin up some threads to do the rendering
    std::vector<std::thread> threads;
    threads.resize(c_numThreads);
    for (std::thread& t : threads)
        t = std::thread(ThreadFunc);

    // keep our program going until it's closed
    WindowFunc();

    // wait for the threads to be done
    for (std::thread& t : threads)
        t.join();
    return 0;
}

/*

NOW:

? why is the image so noisy?
 * you may be doing something wrong with probability distribution stuff.  maybe that's why things are so noisy?

* figure out cosine weighted sampling, it's supposed to make it converge faster

NEXT:
* get BRDFs working
 * for now, just choose BDRF type (reflect, refract, diffuse)
 * then combine them after they are working

* related to refraction:
 * Total internal reflection
 * fresnel

* Objects inside of transparent objects are problematic, need to fix.
  * not sure why!

 BRDF stuff:
 * generalize reflect / refract pulses, or leave alone?
 * and whatever else part of the brdf?
 * roughness?

GRAPHICS FEATURES:
* are you handling BRDF pulses correctly? seems like reflect / refract maybe shouldn't be on par with diffuse.
* maybe make a cube type?  could replace the room walls with a cube then even!
* make it so you can do uniform samples instead of random samples, to have a graphical comparison of results.
 * Should make it obvious why monte carlo is the better way
* fresnel: graphics codex talks about fresnel in material section
* smallpt handles glass vs mirrors vs diffuse surfaces differently
 * https://drive.google.com/file/d/0B8g97JkuSSBwUENiWTJXeGtTOHFmSm51UC01YWtCZw/view
* implement roughness somehow
* try mixing direct illumination with monte carlo like this: https://www.shadertoy.com/view/4tl3z4
* scattering function
* importance sampling
* multiple importance sampling
* dont forget to tone map to get from whatever floating point values back to 0..1 before going to u8
 * saturate towards white!
* bloom (post process)
* CSG
* refraction
* beer's law / internal reflection stuff
* participating media (fog) - maybe have fog volumes? i dunno.
* blue noise sampling?
 * try different jittering patterns
 * could even try blue noise dithered thing!
* subsurface scattering
* bokeh
* depth of field
* motion blur (monte carlo sample in time, not just in space)
* load and render meshes
* textures
* bump mapping
? look up "volumetric path tracing"?  https://en.wikipedia.org/wiki/Volumetric_path_tracing
* area lights and image based lighting? this should just work, by having emissive surfaces / textures.
* chromatic abberation etc (may need to do frequency sampling!!)
* adaptive rendering? render at low res, then progressively higher res? look into how that works.
* red / blue 3d glasses mode
? linearly transformed cosines?
* ggx and spherical harmonics
* ccvt sampling and other stuff from "rolling the dice" siggraph talk
* could look at smallpt line by line for parity
 * https://drive.google.com/file/d/0B8g97JkuSSBwUENiWTJXeGtTOHFmSm51UC01YWtCZw/view
* spatial acceleration structure could be helpful perhaps, especially when more objects added
* get importance sampling working, to get reflection working for scattering / BRDF
* importance sampling: https://inst.eecs.berkeley.edu/~cs294-13/fa09/lectures/scribe-lecture5.pdf
* make it so you can render multiple frames and it puts them together into a video

SCENE:
* add a skybox?

OTHER:
* try to make it so you give a thread an entire row to do.  May be faster?
* do TODO's in code files
* visualize # of raybounces, instead of colors, for complexity analysis?
 * maybe defines or settings to do this?
 * also visualize normals and reflection bounces or something?
* make it so you can animate things & camera over time at a specified frame rate.  write each frame to disk. combine with ffmpeg to make videos!
* aspect ratio support is weird. it stretches images in a funny way.  may be correct?
* profile with sleepy to see where the time is going!
* move window stuff into it's own file?

! blog posts on all this info
 * basic path tracing / rendering equation
 * advanced features like russian roulette and such
 * specific features

*/