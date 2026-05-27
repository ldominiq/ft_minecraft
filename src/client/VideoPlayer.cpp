// #include "VideoPlayer.hpp"
// #include <stdexcept>
// #include <GLFW/glfw3.h>

// #include <GL/gl.h>

// VideoPlayer::VideoPlayer(const std::string& path)
// {
// 	avformat_open_input(&formatCtx, path.c_str(), nullptr, nullptr);
// 	avformat_find_stream_info(formatCtx, nullptr);

// 	for (unsigned i = 0; i < formatCtx->nb_streams; i++)
// 	{
// 		if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
// 		{
// 			videoStream = i;
// 			break;
// 		}
// 	}

// 	if (videoStream == -1)
// 		throw std::runtime_error("No video stream");

// 	// Compute time base for PTS → seconds conversion
// 	AVRational tb = formatCtx->streams[videoStream]->time_base;
// 	timeBase = av_q2d(tb);

// 	auto* codecPar = formatCtx->streams[videoStream]->codecpar;
// 	const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);

// 	codecCtx = avcodec_alloc_context3(codec);
// 	avcodec_parameters_to_context(codecCtx, codecPar);
// 	avcodec_open2(codecCtx, codec, nullptr);

// 	width = codecCtx->width;
// 	height = codecCtx->height;

// 	frame = av_frame_alloc();
// 	frameRGBA = av_frame_alloc();

// 	int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
// 	buffer = (uint8_t*)av_malloc(numBytes);

// 	av_image_fill_arrays(
// 		frameRGBA->data,
// 		frameRGBA->linesize,
// 		buffer,
// 		AV_PIX_FMT_RGBA,
// 		width,
// 		height,
// 		1
// 	);

// 	swsCtx = sws_getContext(
// 		width,
// 		height,
// 		codecCtx->pix_fmt,
// 		width,
// 		height,
// 		AV_PIX_FMT_RGBA,
// 		SWS_BILINEAR,
// 		nullptr,
// 		nullptr,
// 		nullptr
// 	);

// 	// Create texture once
// 	glGenTextures(1, &texture);
// 	glBindTexture(GL_TEXTURE_2D, texture);

// 	glTexImage2D(
// 		GL_TEXTURE_2D,
// 		0,
// 		GL_RGBA,
// 		width,
// 		height,
// 		0,
// 		GL_RGBA,
// 		GL_UNSIGNED_BYTE,
// 		nullptr
// 	);

// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// }

// VideoPlayer::~VideoPlayer()
// {
// 	glDeleteTextures(1, &texture);

// 	av_free(buffer);
// 	av_frame_free(&frame);
// 	av_frame_free(&frameRGBA);

// 	avcodec_free_context(&codecCtx);
// 	avformat_close_input(&formatCtx);

// 	sws_freeContext(swsCtx);
// }

// void VideoPlayer::uploadToGPU()
// {
// 	glBindTexture(GL_TEXTURE_2D, texture);

// 	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

// 	// If linesize matches width * 4 (no padding), just upload directly
// 	// Otherwise, we need to tell OpenGL about the row stride
// 	int rowPixels = frameRGBA->linesize[0] / 4;
// 	if (rowPixels != width)
// 		glPixelStorei(GL_UNPACK_ROW_LENGTH, rowPixels);

// 	glTexSubImage2D(
// 		GL_TEXTURE_2D,
// 		0,
// 		0,
// 		0,
// 		width,
// 		height,
// 		GL_RGBA,
// 		GL_UNSIGNED_BYTE,
// 		frameRGBA->data[0]
// 	);

// 	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
// }

// bool VideoPlayer::decodeNextFrame()
// {
//     AVPacket packet;

//     // Read packets until we get a decoded frame or run out of input
//     while (av_read_frame(formatCtx, &packet) >= 0)
//     {
//         if (packet.stream_index != videoStream)
//         {
//             av_packet_unref(&packet);
//             continue;
//         }

//         int send_ret = avcodec_send_packet(codecCtx, &packet);
//         av_packet_unref(&packet);
//         if (send_ret == AVERROR(EAGAIN) || send_ret < 0)
//             continue;

//         int recv_ret = avcodec_receive_frame(codecCtx, frame);
//         if (recv_ret == AVERROR(EAGAIN) || recv_ret < 0)
//             continue;

//         // Convert to RGBA
//         sws_scale(
//             swsCtx,
//             frame->data,
//             frame->linesize,
//             0,
//             height,
//             frameRGBA->data,
//             frameRGBA->linesize
//         );

//         // Get PTS in seconds, prefer best_effort_timestamp if pts missing
//         int64_t pts = frame->pts;
//         if (pts == AV_NOPTS_VALUE)
//             pts = frame->best_effort_timestamp;
//         if (pts == AV_NOPTS_VALUE)
//             pts = 0;
//         nextFramePTS = pts * timeBase;

//         return true;
//     }

//     // No more input available (EOF or read error) - let caller handle seeking/looping.
//     return false;
// }

// bool VideoPlayer::nextFrame()
// {
//     double now = glfwGetTime();

//     // First call: initialize start time and decode the first frame
//     if (startTime < 0.0)
//     {
//         startTime = now;
//         if (!decodeNextFrame())
//             return false;
//         uploadToGPU();
//         return true;
//     }

//     double elapsed = now - startTime;

//     // If the current frame's PTS is still in the future, don't advance
//     if (elapsed < nextFramePTS)
//         return true; // still showing the current frame

//     // Decode frames until we catch up to the current wall-clock time.
//     // If we hit EOF, perform a single safe seek+flush and continue decoding.
//     bool decoded = false;
//     bool triedSeek = false;
//     while (nextFramePTS <= elapsed)
//     {
//         if (!decodeNextFrame())
//         {
//             if (triedSeek)
//                 return false; // already tried to loop and nothing decoded -> stop

//             // Seek back to start (try stream seek then global), reset decoder timing
//             if (av_seek_frame(formatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD) < 0)
//             {
//                 if (avformat_seek_file(formatCtx, -1, INT64_MIN, 0, INT64_MAX, 0) < 0)
//                     return false; // can't seek -> stop playback
//             }
//             avcodec_flush_buffers(codecCtx);

//             // Restart wall-clock baseline so PTS pacing is relative to now
//             startTime = now;
//             elapsed = 0.0;
//             nextFramePTS = 0.0;
//             triedSeek = true;

//             // Try to decode again from the start of the stream
//             continue;
//         }

//         decoded = true;
//     }

//     // Only upload the latest frame we decoded (skip intermediate ones)
//     if (decoded)
//         uploadToGPU();

//     return true;
// }