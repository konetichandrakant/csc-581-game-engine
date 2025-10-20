// src/Engine/object/PerformanceTest.hpp
#pragma once
#include <chrono>
#include <vector>
#include <string>
#include <fstream>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>

namespace Engine::Obj {

enum class NetworkStrategy {
    FULL_STATE_P2P,     // Current P2P with full state
    INPUT_DELTA_P2P,    // P2P with input deltas only
    FULL_STATE_CS,      // Client-Server with full state
    INPUT_DELTA_CS      // Client-Server with input deltas
};

// Forward declarations

struct PerformanceMetrics {
    std::string strategyName;
    int numClients;
    int numStaticObjects;
    int numMovingObjects;
    int iterations;
    
    double avgTimeMs;
    double minTimeMs;
    double maxTimeMs;
    double variance;
    double stdDev;
    
    size_t totalBytesSent;
    size_t totalMessagesSent;
    double avgBandwidthKbps;
    double avgLatencyMs;
    
    std::vector<double> rawTimes;
};

class PerformanceTestFramework {
public:
    PerformanceTestFramework();
    ~PerformanceTestFramework();
    
    // Test configuration
    void setTestParameters(int iterations, int numRuns);
    void addTestScenario(int clients, int staticObjs, int movingObjs);
    
    // Run performance tests
    void runAllTests();
    void runSingleTest(NetworkStrategy strategy, int clients, int staticObjs, int movingObjs);
    
    // Results
    void saveResults(const std::string& filename);
    void printResults();
    
    struct TestScenario {
        int clients;
        int staticObjects;
        int movingObjects;
    };
    
private:
    
    std::vector<TestScenario> scenarios_;
    std::vector<PerformanceMetrics> results_;
    
    int iterations_{100000};
    int numRuns_{5};
    
    std::mutex resultsMutex_;
    
    // Performance tracking
    size_t totalBytesSent_;
    size_t totalMessagesSent_;
    double avgLatencyMs_;
    std::unordered_map<int, uint8_t> lastInputStates_;
    
    // Test execution
    PerformanceMetrics runTestScenario(NetworkStrategy strategy, const TestScenario& scenario);
    void setupTestEnvironment(NetworkStrategy strategy, const TestScenario& scenario);
    void cleanupTestEnvironment();
    
    // Metrics collection
    void collectMetrics(PerformanceMetrics& metrics, const std::chrono::steady_clock::time_point& start,
                       const std::chrono::steady_clock::time_point& end);
    
    // Statistics
    double calculateVariance(const std::vector<double>& times, double mean);
    double calculateStdDev(double variance);
    std::string getStrategyName(NetworkStrategy strategy);
    
    // Simulation methods
    void simulateGameTick(NetworkStrategy strategy, const TestScenario& scenario);
    void simulateFullStateP2P(const TestScenario& scenario);
    void simulateInputDeltaP2P(const TestScenario& scenario);
    void simulateFullStateCS(const TestScenario& scenario);
    void simulateInputDeltaCS(const TestScenario& scenario);
    
    // Setup methods
    void setupFullStateP2P(const TestScenario& scenario);
    void setupInputDeltaP2P(const TestScenario& scenario);
    void setupFullStateCS(const TestScenario& scenario);
    void setupInputDeltaCS(const TestScenario& scenario);
    
    // Message sending methods
    void sendFullStateMessage(int from, int to, float x, float y, float vx, float vy, uint8_t facing, uint8_t anim);
    void sendObjectStateMessage(int objectId, float x, float y);
    void sendInputDeltaMessage(int from, int to, uint8_t inputFlags);
    void reconstructPlayerState(int clientId);
};

} // namespace Engine::Obj