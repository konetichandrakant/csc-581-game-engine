#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace Engine {

    inline constexpr int HANDSHAKE_PORT     = 5555;
    inline constexpr int FIRST_CLIENT_PORT  = 5600;

    struct XY { float x=0, y=0; };

    // ==== wire helpers ====
    struct Blob { std::vector<std::uint8_t> data; };
    template<class T>
    inline Blob to_blob(const T& pod){ static_assert(std::is_trivially_copyable_v<T>);
        Blob b; b.data.resize(sizeof(T)); std::memcpy(b.data.data(), &pod, sizeof(T)); return b; }
    template<class T>
    inline T from_blob(const void* p, size_t n){
        if(n!=sizeof(T)) throw std::runtime_error("bad blob");
        T t{}; std::memcpy(&t,p,sizeof(T)); return t;
    }

    // ==== messages ====
    enum class MsgKind : std::uint32_t { Handshake=1, Pos=2, Ping=3 };

    struct HandshakeReq { std::uint32_t kind=(std::uint32_t)MsgKind::Handshake; std::uint32_t requested_id=0; };
    struct HandshakeRep { std::uint32_t assigned_id=0; std::uint32_t port=0; };

    struct PosMsg { std::uint32_t kind=(std::uint32_t)MsgKind::Pos; std::uint32_t client_id=0; float x=0, y=0; };

    // Snapshot layout:
    // [SnapshotHdr]
    // [num_players * (uint32_t id + XY pos)]
    // [num_platforms * XY pos]
    struct SnapshotHdr {
        std::uint32_t tick=0;
        std::uint32_t num_players=0;
        std::uint32_t num_platforms=0;
    };

} // namespace Engine
