// src/Engine/object/PerformanceTest.cpp
#include "PerformanceTest.hpp"
#include "NetworkSceneManager.hpp"
#include "InputDeltaNetwork.hpp"
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <sstream>
#include <random>
#include <thread>
#include <chrono>
#include <iostream>
#include <cmath>

namespace Engine::Obj {

PerformanceTestFramework::PerformanceTestFramework() = default;

PerformanceTestFramework::~PerformanceTestFramework() = default;

void PerformanceTestFramework::setTestParameters(int iterations, int numRuns) {
    iterations_ = iterations;
    numRuns_ = numRuns;
}

void PerformanceTestFramework::addTestScenario(int clients, int staticObjs, int movingObjs) {
    scenarios_.push_back({clients, staticObjs, movingObjs});
}

void PerformanceTestFramework::runAllTests() {
    std::vector<NetworkStrategy> strategies = {
        NetworkStrategy::FULL_STATE_P2P,
        NetworkStrategy::INPUT_DELTA_P2P,
        NetworkStrategy::FULL_STATE_CS,
        NetworkStrategy::INPUT_DELTA_CS
    };
    
    for (auto strategy : strategies) {
        for (const auto& scenario : scenarios_) {
            std::cout << "Running test: " << static_cast<int>(strategy) 
                      << " with " << scenario.clients << " clients, "
                      << scenario.staticObjects << " static, "
                      << scenario.movingObjects << " moving objects\n";
            
            runSingleTest(strategy, scenario.clients, scenario.staticObjects, scenario.movingObjects);
        }
    }
}

void PerformanceTestFramework::runSingleTest(NetworkStrategy strategy, int clients, int staticObjs, int movingObjs) {
    TestScenario scenario{clients, staticObjs, movingObjs};
    PerformanceMetrics metrics = runTestScenario(strategy, scenario);
    
    std::lock_guard<std::mutex> lock(resultsMutex_);
    results_.push_back(metrics);
}

PerformanceMetrics PerformanceTestFramework::runTestScenario(NetworkStrategy strategy, const TestScenario& scenario) {
    PerformanceMetrics metrics;
    
    // Set up metrics
    metrics.strategyName = getStrategyName(strategy);
    metrics.numClients = scenario.clients;
    metrics.numStaticObjects = scenario.staticObjects;
    metrics.numMovingObjects = scenario.movingObjects;
    metrics.iterations = iterations_;
    
    std::vector<double> runTimes;
    runTimes.reserve(numRuns_);
    
    for (int run = 0; run < numRuns_; ++run) {
        setupTestEnvironment(strategy, scenario);
        
        auto start = std::chrono::steady_clock::now();
        
        // Simulate game loop iterations
        for (int i = 0; i < iterations_; ++i) {
            simulateGameTick(strategy, scenario);
        }
        
        auto end = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double timeMs = duration.count() / 1000.0;
        runTimes.push_back(timeMs);
        
        cleanupTestEnvironment();
    }
    
    // Calculate statistics
    metrics.rawTimes = runTimes;
    metrics.avgTimeMs = std::accumulate(runTimes.begin(), runTimes.end(), 0.0) / numRuns_;
    metrics.minTimeMs = *std::min_element(runTimes.begin(), runTimes.end());
    metrics.maxTimeMs = *std::max_element(runTimes.begin(), runTimes.end());
    metrics.variance = calculateVariance(runTimes, metrics.avgTimeMs);
    metrics.stdDev = calculateStdDev(metrics.variance);
    
    return metrics;
}

void PerformanceTestFramework::setupTestEnvironment(NetworkStrategy strategy, const TestScenario& scenario) {
    // Initialize test environment based on strategy
    switch (strategy) {
        case NetworkStrategy::FULL_STATE_P2P:
            setupFullStateP2P(scenario);
            break;
        case NetworkStrategy::INPUT_DELTA_P2P:
            setupInputDeltaP2P(scenario);
            break;
        case NetworkStrategy::FULL_STATE_CS:
            setupFullStateCS(scenario);
            break;
        case NetworkStrategy::INPUT_DELTA_CS:
            setupInputDeltaCS(scenario);
            break;
    }
}

void PerformanceTestFramework::simulateGameTick(NetworkStrategy strategy, const TestScenario& scenario) {
    // Simulate one game tick based on strategy
    switch (strategy) {
        case NetworkStrategy::FULL_STATE_P2P:
            simulateFullStateP2P(scenario);
            break;
        case NetworkStrategy::INPUT_DELTA_P2P:
            simulateInputDeltaP2P(scenario);
            break;
        case NetworkStrategy::FULL_STATE_CS:
            simulateFullStateCS(scenario);
            break;
        case NetworkStrategy::INPUT_DELTA_CS:
            simulateInputDeltaCS(scenario);
            break;
    }
}

void PerformanceTestFramework::cleanupTestEnvironment() {
    // Clean up test environment
}

void PerformanceTestFramework::collectMetrics(PerformanceMetrics& metrics, const std::chrono::steady_clock::time_point& start,
                                          const std::chrono::steady_clock::time_point& end) {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double timeSeconds = duration.count() / 1000.0;

    metrics.totalBytesSent = totalBytesSent_;
    metrics.totalMessagesSent = totalMessagesSent_;
    metrics.avgLatencyMs = avgLatencyMs_;

    // Calculate bandwidth in Kbps
    if (timeSeconds > 0) {
        metrics.avgBandwidthKbps = (totalBytesSent_ * 8.0) / (timeSeconds * 1000.0);
    }
}

// ================== FULL STATE P2P SIMULATION ==================

void PerformanceTestFramework::setupFullStateP2P(const TestScenario& scenario) {
    // Initialize P2P simulation
    totalBytesSent_ = 0;
    totalMessagesSent_ = 0;
    avgLatencyMs_ = 0.0;
}

void PerformanceTestFramework::simulateFullStateP2P(const TestScenario& scenario) {
    // Simulate full state broadcasting in P2P
    for (int client = 0; client < scenario.clients; ++client) {
        // Generate random player state
        float x = rand() % 1920;
        float y = rand() % 1080;
        float vx = (rand() % 200) - 100;
        float vy = (rand() % 200) - 100;
        uint8_t facing = rand() % 2;
        uint8_t anim = rand() % 4;
        
        // Send full state to all other clients
        for (int other = 0; other < scenario.clients; ++other) {
            if (other != client) {
                // Simulate network send
                sendFullStateMessage(client, other, x, y, vx, vy, facing, anim);
            }
        }
    }
    
    // Simulate object updates
    for (int i = 0; i < scenario.movingObjects; ++i) {
        // Send object state updates
        sendObjectStateMessage(i, rand() % 1920, rand() % 1080);
    }
}

void PerformanceTestFramework::sendFullStateMessage(int from, int to, float x, float y, float vx, float vy, uint8_t facing, uint8_t anim) {
    // Simulate full state message (20 bytes)
    struct FullStateMessage {
        uint32_t playerId;
        float x, y, vx, vy;
        uint8_t facing, anim;
        uint8_t padding[2];
    };
    
    FullStateMessage msg{static_cast<uint32_t>(from), x, y, vx, vy, facing, anim, {0, 0}};
    
    totalBytesSent_ += sizeof(FullStateMessage);
    totalMessagesSent_++;
    
    // Simulate network latency (1-5ms)
    int latency = 1 + (rand() % 5);
    avgLatencyMs_ = (avgLatencyMs_ + latency) / 2.0;
    
    // Simulate processing time
    std::this_thread::sleep_for(std::chrono::microseconds(10));
}

// ================== INPUT DELTA P2P SIMULATION ==================

void PerformanceTestFramework::setupInputDeltaP2P(const TestScenario& scenario) {
    // Initialize input delta simulation
    totalBytesSent_ = 0;
    totalMessagesSent_ = 0;
    avgLatencyMs_ = 0.0;
    
    // Initialize input states
    lastInputStates_.clear();
    for (int i = 0; i < scenario.clients; ++i) {
        lastInputStates_[i] = 0;
    }
}

void PerformanceTestFramework::simulateInputDeltaP2P(const TestScenario& scenario) {
    // Simulate input delta compression in P2P
    for (int client = 0; client < scenario.clients; ++client) {
        // Generate random input (only 10% of frames have input changes)
        if (rand() % 10 == 0) {
            uint8_t inputFlags = rand() % 8; // 3 bits for left, right, jump
            
            // Only send if input changed
            if (inputFlags != lastInputStates_[client]) {
                // Send input delta to all other clients
                for (int other = 0; other < scenario.clients; ++other) {
                    if (other != client) {
                        sendInputDeltaMessage(client, other, inputFlags);
                    }
                }
                lastInputStates_[client] = inputFlags;
            }
        }
    }
    
    // Simulate state reconstruction (CPU intensive)
    for (int client = 0; client < scenario.clients; ++client) {
        reconstructPlayerState(client);
    }
}

void PerformanceTestFramework::sendInputDeltaMessage(int from, int to, uint8_t inputFlags) {
    // Simulate input delta message (10 bytes)
    struct InputDeltaMessage {
        uint32_t playerId;
        uint8_t inputFlags;
        uint8_t sequence;
        uint8_t padding[4];
    };
    
    InputDeltaMessage msg{static_cast<uint32_t>(from), inputFlags, 0, {0, 0, 0, 0}};
    
    totalBytesSent_ += sizeof(InputDeltaMessage);
    totalMessagesSent_++;
    
    // Simulate network latency (2-8ms - higher due to processing)
    int latency = 2 + (rand() % 7);
    avgLatencyMs_ = (avgLatencyMs_ + latency) / 2.0;
    
    // Simulate processing time
    std::this_thread::sleep_for(std::chrono::microseconds(20));
}

void PerformanceTestFramework::reconstructPlayerState(int clientId) {
    // Simulate physics reconstruction (CPU intensive)
    // This is where input deltas get converted back to player positions
    for (int i = 0; i < 100; ++i) {
        // Simulate physics calculations
        volatile float x = rand() % 1920;
        volatile float y = rand() % 1080;
        volatile float vx = (rand() % 200) - 100;
        volatile float vy = (rand() % 200) - 100;
        
        // Simulate collision detection
        if (x < 0) x = 0;
        if (x > 1920) x = 1920;
        if (y < 0) y = 0;
        if (y > 1080) y = 1080;
    }
}

// ================== CLIENT-SERVER SIMULATIONS ==================

void PerformanceTestFramework::setupFullStateCS(const TestScenario& scenario) {
    setupFullStateP2P(scenario);
}

void PerformanceTestFramework::simulateFullStateCS(const TestScenario& scenario) {
    // Simulate client-server with full state
    // Clients send to server, server broadcasts to all clients
    for (int client = 0; client < scenario.clients; ++client) {
        // Client sends to server
        sendFullStateMessage(client, -1, rand() % 1920, rand() % 1080, 
                           (rand() % 200) - 100, (rand() % 200) - 100, 
                           rand() % 2, rand() % 4);
    }
    
    // Server broadcasts to all clients
    for (int client = 0; client < scenario.clients; ++client) {
        for (int other = 0; other < scenario.clients; ++other) {
            if (other != client) {
                sendFullStateMessage(-1, other, rand() % 1920, rand() % 1080,
                                   (rand() % 200) - 100, (rand() % 200) - 100,
                                   rand() % 2, rand() % 4);
            }
        }
    }
}

void PerformanceTestFramework::setupInputDeltaCS(const TestScenario& scenario) {
    setupInputDeltaP2P(scenario);
}

void PerformanceTestFramework::simulateInputDeltaCS(const TestScenario& scenario) {
    // Simulate client-server with input deltas
    // Clients send input to server, server processes and broadcasts
    for (int client = 0; client < scenario.clients; ++client) {
        if (rand() % 10 == 0) {
            uint8_t inputFlags = rand() % 8;
            if (inputFlags != lastInputStates_[client]) {
                // Client sends to server
                sendInputDeltaMessage(client, -1, inputFlags);
                lastInputStates_[client] = inputFlags;
            }
        }
    }
    
    // Server processes and broadcasts to all clients
    for (int client = 0; client < scenario.clients; ++client) {
        reconstructPlayerState(client);
        for (int other = 0; other < scenario.clients; ++other) {
            if (other != client) {
                sendInputDeltaMessage(-1, other, rand() % 8);
            }
        }
    }
}

void PerformanceTestFramework::sendObjectStateMessage(int objectId, float x, float y) {
    // Simulate object state message (12 bytes)
    struct ObjectStateMessage {
        uint32_t objectId;
        float x, y;
        uint8_t padding[4];
    };
    
    ObjectStateMessage msg{static_cast<uint32_t>(objectId), x, y, {0, 0, 0, 0}};
    
    totalBytesSent_ += sizeof(ObjectStateMessage);
    totalMessagesSent_++;
}

std::string PerformanceTestFramework::getStrategyName(NetworkStrategy strategy) {
    switch (strategy) {
        case NetworkStrategy::FULL_STATE_P2P: return "Full State P2P";
        case NetworkStrategy::INPUT_DELTA_P2P: return "Input Delta P2P";
        case NetworkStrategy::FULL_STATE_CS: return "Full State Client-Server";
        case NetworkStrategy::INPUT_DELTA_CS: return "Input Delta Client-Server";
        default: return "Unknown";
    }
}

double PerformanceTestFramework::calculateVariance(const std::vector<double>& times, double mean) {
    double sum = 0.0;
    for (double time : times) {
        double diff = time - mean;
        sum += diff * diff;
    }
    return sum / times.size();
}

double PerformanceTestFramework::calculateStdDev(double variance) {
    return std::sqrt(variance);
}

void PerformanceTestFramework::saveResults(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }
    
    // CSV header
    file << "Strategy,Clients,StaticObjects,MovingObjects,Iterations,"
         << "AvgTimeMs,MinTimeMs,MaxTimeMs,Variance,StdDev,"
         << "TotalBytes,TotalMessages,AvgBandwidthKbps,AvgLatencyMs\n";
    
    std::lock_guard<std::mutex> lock(resultsMutex_);
    for (const auto& result : results_) {
        file << result.strategyName << "," << result.numClients << ","
             << result.numStaticObjects << "," << result.numMovingObjects << ","
             << result.iterations << "," << result.avgTimeMs << ","
             << result.minTimeMs << "," << result.maxTimeMs << ","
             << result.variance << "," << result.stdDev << ","
             << result.totalBytesSent << "," << result.totalMessagesSent << ","
             << result.avgBandwidthKbps << "," << result.avgLatencyMs << "\n";
    }
}

void PerformanceTestFramework::printResults() {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    
    std::cout << "\n=== PERFORMANCE TEST RESULTS ===\n";
    std::cout << std::fixed << std::setprecision(2);
    
    for (const auto& result : results_) {
        std::cout << "\nStrategy: " << result.strategyName << "\n";
        std::cout << "Clients: " << result.numClients 
                  << ", Static: " << result.numStaticObjects
                  << ", Moving: " << result.numMovingObjects << "\n";
        std::cout << "Avg Time: " << result.avgTimeMs << "ms\n";
        std::cout << "Min/Max: " << result.minTimeMs << "ms / " << result.maxTimeMs << "ms\n";
        std::cout << "Std Dev: " << result.stdDev << "ms\n";
        std::cout << "Bandwidth: " << result.avgBandwidthKbps << " Kbps\n";
        std::cout << "Latency: " << result.avgLatencyMs << "ms\n";
    }
}

} // namespace Engine::Obj