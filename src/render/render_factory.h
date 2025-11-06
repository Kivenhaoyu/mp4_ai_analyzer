//
//  render_factory.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 24/10/2025.
//

#ifndef RENDER_FACTORY_H
#define RENDER_FACTORY_H

#pragma once

#include "renderer.h"
#include "../common/platform.h"

#ifdef PLATFORM_MAC
    #include "platform/mac/mac_renderer.h"
#elif PLATFORM_LINUX

#elif PLATFORM_WIN

#else
        throw std::runtime_error("Unsupported platform");
#endif


class RendererFactory {
public:
    static std::unique_ptr<Renderer> createRenderer() {
#ifdef PLATFORM_MAC
        return std::make_unique<MACFrameRenderer>();
#elif PLATFORM_LINUX
//        return std::make_unique<LinuxFrameRenderer>(window_name, width, height);
#elif PLATFORM_WIN
//        return std::make_unique<WinFrameRenderer>(window_name, width, height);
#else
        throw std::runtime_error("Unsupported platform");
#endif
    }
};


#endif /* RENDER_FACTORY_H */
