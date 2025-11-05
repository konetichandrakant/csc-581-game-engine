#pragma once
#include <cstdint>
#include <cstring>

namespace Engine { namespace Net {

    constexpr uint32_t PROTO_VER = 1;
    constexpr uint32_t MAX_PLAYERS = 8;
    constexpr uint32_t MAX_PLATFORMS = 16;

    enum class MsgKind : uint8_t { Input=1, State=2, Hello=3, HelloAck=4 };

    struct Vec2 { float x, y; };

    struct InputMsg {
        uint32_t proto_ver;
        MsgKind  kind;
        uint32_t client_id;
        uint64_t input_seq;
        float    dt_client;
        uint8_t  left;
        uint8_t  right;
        uint8_t  jump;
        uint8_t  _pad{0};

        static InputMsg make(uint32_t id, uint64_t seq, float dt, bool L, bool R, bool J) {
            InputMsg m{};
            m.proto_ver = PROTO_VER;
            m.kind = MsgKind::Input;
            m.client_id = id;
            m.input_seq = seq;
            m.dt_client = dt;
            m.left  = L?1:0;
            m.right = R?1:0;
            m.jump  = J?1:0;
            return m;
        }
    };

    struct PlayerState {
        uint32_t client_id;
        Vec2     pos;
        Vec2     vel;
    };

    struct PlatformState {
        uint32_t id;
        Vec2     pos;
    };

    struct StateMsg {
        uint32_t proto_ver;
        MsgKind  kind;
        uint64_t world_tick;
        double   world_time;
        PlayerState me;
        uint32_t others_count;
        uint32_t platforms_count;
        PlayerState others[MAX_PLAYERS];
        PlatformState platforms[MAX_PLATFORMS];

        void clear() {
            std::memset(this, 0, sizeof(*this));
            proto_ver = PROTO_VER;
            kind = MsgKind::State;
        }
    };

}}
