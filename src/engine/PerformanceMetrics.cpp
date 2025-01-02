#include "Renderer.h"

void PerformanceMetrics::updateMemoryUsage(size_t ram, size_t vram) {
    ramUsageMB = ram;
    vramUsageMB = vram;
    
    // Update history
    ramHistory1Min.push_back(ram);
    vramHistory1Min.push_back(vram);
    if (ramHistory1Min.size() > ONE_MINUTE_SAMPLES) {
        ramHistory1Min.pop_front();
    }
    if (vramHistory1Min.size() > ONE_MINUTE_SAMPLES) {
        vramHistory1Min.pop_front();
    }
    
    // Update peaks
    peakRAM = std::max(peakRAM, ram);
    peakVRAM = std::max(peakVRAM, vram);
}

void PerformanceMetrics::updateFPS(float fps) {
    // Update 1-minute history
    fpsHistory1Min.push_back(fps);
    if (fpsHistory1Min.size() > ONE_MINUTE_SAMPLES) {
        fpsHistory1Min.pop_front();
    }
    
    // Update 5-minute history
    fpsHistory5Min.push_back(fps);
    if (fpsHistory5Min.size() > FIVE_MINUTE_SAMPLES) {
        fpsHistory5Min.pop_front();
    }
    
    // Update min/max/avg
    minFPS = std::min(minFPS, fps);
    maxFPS = std::max(maxFPS, fps);
    avgFPS = 0.0f;
    for (float f : fpsHistory1Min) {
        avgFPS += f;
    }
    avgFPS /= fpsHistory1Min.size();
}

void PerformanceMetrics::updateUPS(float ups) {
    // Update 1-minute history
    upsHistory1Min.push_back(ups);
    if (upsHistory1Min.size() > ONE_MINUTE_SAMPLES) {
        upsHistory1Min.pop_front();
    }
    
    // Update 5-minute history
    upsHistory5Min.push_back(ups);
    if (upsHistory5Min.size() > FIVE_MINUTE_SAMPLES) {
        upsHistory5Min.pop_front();
    }
    
    // Update min/max/avg
    minUPS = std::min(minUPS, ups);
    maxUPS = std::max(maxUPS, ups);
    avgUPS = 0.0f;
    for (float u : upsHistory1Min) {
        avgUPS += u;
    }
    avgUPS /= upsHistory1Min.size();
}

std::vector<float> PerformanceMetrics::getFPSHistory1MinVector() const {
    return std::vector<float>(fpsHistory1Min.begin(), fpsHistory1Min.end());
}

std::vector<float> PerformanceMetrics::getUPSHistory1MinVector() const {
    return std::vector<float>(upsHistory1Min.begin(), upsHistory1Min.end());
}

std::vector<float> PerformanceMetrics::getFPSHistory5MinVector() const {
    return std::vector<float>(fpsHistory5Min.begin(), fpsHistory5Min.end());
}

std::vector<float> PerformanceMetrics::getUPSHistory5MinVector() const {
    return std::vector<float>(upsHistory5Min.begin(), upsHistory5Min.end());
} 