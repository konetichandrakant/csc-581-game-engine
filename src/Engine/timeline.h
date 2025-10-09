#pragma once
#include <string>
#include <chrono>

typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long, std::ratio<1, 1000000000>>> time_point;

namespace Engine {

    /**
     * Timeline represents a controllable clock that can run in sync
     * with real time or another parent timeline.
     * It supports pausing, scaling, and anchoring to another timeline.
     */
    class Timeline {
    public:
        Timeline(const std::string& name = "Timeline");
        /**
         * call this once per frame to get accurate per-tick time deltas
         * and update the timeline.
         */
        void tick();
        void setScale(double s);
        double scale() const;

        void pause();
        void unpause();
        void togglePause();
        bool isPaused() const;

        double getDelta() const;
        double now() const;
        void reset();

    private:
        std::string _name;      // Human-readable name of the timeline.
        double _scale = 1.0;    // Speed multiplier.
        bool   _paused = false; // Whether the timeline is currently paused.
        double _delta = 0.0;    // Delta computed in last update.
        double _accum = 0.0;    // Total accumulated time.
        time_point _last_t;     // timestamp of last frame (used for computing delta)
    };

} // namespace Engine
