// src/performance_test.cpp
#include "Engine/object/PerformanceTest.hpp"
#include "Engine/object/InputDeltaNetwork.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "Starting Performance Comparison Framework\n";

    Engine::Obj::PerformanceTestFramework framework;

    // Configure test parameters
    framework.setTestParameters(10000, 3); // 100k iterations, 5 runs each

    // Add test scenarios
    framework.addTestScenario(2, 10, 10);   // 2 clients, 10 static, 10 moving
    framework.addTestScenario(4, 50, 50);   // 4 clients, 50 static, 50 moving
    framework.addTestScenario(8, 100, 100); // 8 clients, 100 static, 100 moving
    framework.addTestScenario(16, 200, 200); // 16 clients, 200 static, 200 moving

    // Run all tests
    auto start = std::chrono::steady_clock::now();
    framework.runAllTests();
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::minutes>(end - start);
    std::cout << "Total test time: " << duration.count() << " minutes\n";

    // Save and display results
    framework.saveResults("performance_results.csv");
    framework.printResults();

    std::cout << "Performance testing complete!\n";
    return 0;
}