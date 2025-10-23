#include "Engine/object/PerformanceTest.hpp"
#include "Engine/object/InputDeltaNetwork.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <tuple>

int main() {
    using namespace std::chrono;

    constexpr int ITERATIONS_PER_TRIAL = 100000; // 100k iterations per trial
    constexpr int TRIALS_PER_CONDITION = 5;      // 5 trials per condition

    std::cout << "Starting Performance Comparison Framework\n";

    Engine::Obj::PerformanceTestFramework framework;

    // Apply parameters to framework
    framework.setTestParameters(ITERATIONS_PER_TRIAL, TRIALS_PER_CONDITION);

    const std::vector<std::tuple<int,int,int>> scenarios = {
        {  2,  10,  10},
        {  4,  50,  50},
        {  8, 100, 100},
        { 16, 200, 200},
    };

    std::cout << "\n=== RUN CONFIGURATION ===\n";
    std::cout << "Trials per condition: "      << TRIALS_PER_CONDITION     << "\n";
    std::cout << "Iterations per trial: "      << ITERATIONS_PER_TRIAL     << "\n";
    std::cout << "Number of scenarios: "       << scenarios.size()         << "\n";
    std::cout << "Scenarios:\n";
    for (size_t i = 0; i < scenarios.size(); ++i) {
        auto [clients, statics, movings] = scenarios[i];
        std::cout << "  " << i+1 << ") Clients=" << clients
                  << ", Static=" << statics
                  << ", Moving=" << movings << "\n";
    }
    std::cout << "====================================\n\n";

    for (auto [clients, statics, movings] : scenarios) {
        framework.addTestScenario(clients, statics, movings);
    }

    // Run all tests
    const auto start = steady_clock::now();
    framework.runAllTests();
    const auto end   = steady_clock::now();

    const auto minutes_total = duration_cast<minutes>(end - start);
    std::cout << "Total test time: " << minutes_total.count() << " minutes\n";

    // Save and display results
    framework.saveResults("performance_results.csv");
    framework.printResults();

    std::cout << "Performance testing complete!\n";
    return 0;
}
