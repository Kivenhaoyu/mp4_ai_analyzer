//
//  audio_decoder.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 28/10/2025.
//

#ifndef AUDIO_DECODER_H
#define AUDIO_DECODER_H

#include <memory>


struct AudioFrame {
    uint8_t* data;
    int linesize;
    int sample_rate;
    int channels;
    int format;
    int nb_samples;
};

using AudioFramePtr = std::shared_ptr<AudioFrame>


#endif /* AUDIO_DECODER_H */
