#include "timeline.h"
#include <algorithm>
#include <chrono>
#include <iostream>

namespace Engine {

    Timeline::Timeline(const std::string& name) : _name(name) {
        _last_t = std::chrono::system_clock::now();
        _delta = 0.016;
        _accum = 0.0;
    }

    void Timeline::tick() {
        auto now = std::chrono::system_clock::now();


        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_t);
        double frame_time = duration.count() / 1000.0;

        _last_t = now;


        if (frame_time > 0.033) frame_time = 0.033;
        if (frame_time < 0.001) frame_time = 0.016;

        if (_paused) {
            _delta = 0.0;
        } else {
            _delta = frame_time * _scale;
            _accum += _delta;
        }
    }

    void Timeline::setScale(double s) {
        _scale = std::max(0.0, s);
    }

    double Timeline::scale() const {
        return _scale;
    }

    void Timeline::pause() { _paused = true; }
    void Timeline::unpause() { _paused = false; }
    void Timeline::togglePause() { _paused = !_paused; }
    bool Timeline::isPaused() const { return _paused; }
    double Timeline::getDelta() const { return _delta; }
    double Timeline::now() const { return _accum; }

    void Timeline::reset() {
        _delta = 0.016;
        _accum = 0.0;
        _last_t = std::chrono::system_clock::now();
    }
}