#include "simulator.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <unistd.h>

std::atomic<bool> shouldExit(false);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down simulator..." << std::endl;
        shouldExit.store(true);
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    SimulatorConfig config = parseCommandLine(argc, argv);

    MarketDataSimulator simulator(config);

    std::cout << "Starting Market Data Simulator\n"
              << "  Port: " << config.port << "\n"
              << "  Symbol: " << config.symbol << "\n"
              << "  Scenario: ";

    switch (config.scenario) {
        case MarketScenario::STEADY:
            std::cout << "Steady";
            break;
        case MarketScenario::TRENDING_UP:
            std::cout << "Trending Up";
            break;
        case MarketScenario::TRENDING_DOWN:
            std::cout << "Trending Down";
            break;
        case MarketScenario::VOLATILE:
            std::cout << "Volatile";
            break;
        case MarketScenario::CSV_REPLAY:
            std::cout << "CSV Replay";
            break;
    }

    std::cout << "\n  Base Price: $" << config.basePrice
              << "\n  Rate: " << config.messagesPerSecond << " msgs/sec"
              << "\n  Duration: ";

    if (config.duration == 0) {
        std::cout << "Infinite (press Ctrl+C to stop)";
    } else {
        std::cout << config.duration << " seconds";
    }

    std::cout << "\n\n";

    if (!simulator.start()) {
        std::cerr << "Failed to start simulator" << std::endl;
        return 1;
    }

    std::cout << "Simulator is running. Waiting for connections..." << std::endl;

    while (!shouldExit.load() && simulator.isRunning()) {
        usleep(100000); // 100ms
    }

    simulator.stop();
    std::cout << "Simulator stopped." << std::endl;

    return 0;
}
