#ifndef VIDEOPLAYER_HPP
#define VIDEOPLAYER_HPP

// !! WARNING: THIS CLASS HAS BEEN COMPLETELY VIBE CODED !!

#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class VideoPlayer
{
public:

    VideoPlayer(const std::string& path);
    ~VideoPlayer();

    bool nextFrame();
    unsigned int getTexture() const { return texture; }

private:

    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;

    AVFrame* frame = nullptr;
    AVFrame* frameRGBA = nullptr;

    SwsContext* swsCtx = nullptr;

    int videoStream = -1;

    unsigned int texture = 0;

    int width;
    int height;

    uint8_t* buffer = nullptr;

    // Frame pacing
    double timeBase = 0.0;       // seconds per PTS unit
    double startTime = -1.0;     // wall-clock time when playback started
    double nextFramePTS = 0.0;   // PTS of the next frame to display (in seconds)

    bool decodeNextFrame();      // internal: decode one frame from the stream
    void uploadToGPU();          // internal: upload current frameRGBA to GL texture
};

#endif