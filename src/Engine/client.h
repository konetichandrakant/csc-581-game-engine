#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

namespace Engine {


struct XY { float x{0}, y{0}; };


struct RemotePeer {
    int id{0};
    std::atomic<float>    x{0}, y{0}, vx{0}, vy{0};
    std::atomic<uint8_t>  facing{0}, anim{0};
    std::atomic<uint64_t> lastTick{0};
    std::atomic<int64_t>  lastRecvNs{0};
};


struct RemotePeerData {
    int      id = 0;
    float    x  = 0.f, y  = 0.f, vx = 0.f, vy = 0.f;
    uint8_t  facing = 0,  anim = 0;
    uint64_t lastTick = 0;
};

class Client {
public:
    Client();
    ~Client();


    bool start(const std::string& host, const std::string& displayName);
    int  myId() const { return my_id_; }


    void sendPos(float x, float y);


    std::unordered_map<int, XY> snapshot() const;
    std::vector<XY>             platforms() const;



    bool startP2P(const std::string& dirHost, int /*worldPubPortUnused*/, int dirPort);
    void stopP2P();


    void p2pPublishPlayer(uint64_t tick,
                          float x,float y,float vx,float vy,
                          uint8_t facing,uint8_t anim);


    std::unordered_map<int, RemotePeerData> p2pSnapshot();


    void p2pPublishEvent(uint32_t eventKind, float x, float y, const char* extraData);


    struct NetworkEventData {
        uint32_t eventKind;
        float x, y;
        std::string extraData;
        int playerId;
    };
    std::vector<NetworkEventData> getPendingNetworkEvents();


    void configureAuthorityLayout(int winW, int winH);


    void shutdown();

private:

    bool hello(const std::string& host, const std::string& displayName, int cmdPort, int worldPubPort);
    void recvLoop();


    bool p2pQueryDirectoryAndConnect_();
    void p2pRxLoop_();


    void authorityMaybeStepAndBroadcast_();
    void becomeAuthority_();
    void resignAuthority_();

private:

    void* ctx_{nullptr};
    void* req_{nullptr};
    void* sub_{nullptr};
    void* subPeers_{nullptr};
    void* pubMine_{nullptr};


    std::atomic<bool> running_{false};
    std::thread       recv_thread_;
    std::atomic<int>  my_id_{0};


    mutable std::mutex snap_mx_;
    std::unordered_map<int, XY> snap_;
    mutable std::mutex plat_mx_;
    std::vector<XY> platforms_;
    std::atomic<int64_t> lastWorldRecvNs_{0};
    std::atomic<int64_t> lastP2PWorldRecvNs_{0};


    std::atomic<bool> p2pRunning_{false};
    std::thread       p2pRxThread_;
    std::string       dirHost_;
    int               dirPort_{0};
    int               myPubPort_{0};

    std::mutex peersMtx_;
    std::unordered_map<int, RemotePeer> peers_;
    std::unordered_set<int> connectedPeerIds_;
    std::chrono::steady_clock::time_point nextDirRefresh_{};


    mutable std::mutex networkEventsMtx_;
    std::vector<NetworkEventData> pendingNetworkEvents_;


    std::atomic<bool> isAuthority_{false};
    std::atomic<bool> hadServer_{false};
    int winW_{1920}, winH_{1080};

    struct AuthPlat { float x,y,minX,maxX,vx,vy; };
    std::vector<AuthPlat> authPlats_;
    std::chrono::steady_clock::time_point nextAuthSim_{};
    std::chrono::steady_clock::time_point nextAuthPub_{};
};

}
