//
//  platform.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 19/10/2025.
//

#pragma once

#ifdef __APPLE__
#define PLATFORM_MAC 1
#elif defined(__linux__)
#define PLATFORM_LINUX 1
#elif defined(_WIN32)
#define PLATFORM_WIN 1
#else
#error "Unsupported platform"
#endif
