#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <chrono>
#include <iomanip>
#include "config.h"
#include "order_manager.h"
#include "network_manager.h"
#include "message.h"
#include "metrics.h"
#include "runtime_config.h"
#include <thread>
#include <cstdlib>
#ifdef __APPLE__
#include <pthread.h>
#endif

volatile sig_atomic_t g_shutdown_requested = 0;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_shutdown_requested = 1;
        std::cout << "\nShutdown requested..." << std::endl;
    }
}

void setup_signal_handlers() {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    #ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    #endif
}

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name
              << " <symbol> <side> <max_order_size> <vwap_window_seconds>"
              << " <market_data_ip> <market_data_port>"
              << " <order_ip> <order_port>" << std::endl;
    std::cerr << "\nParameters:" << std::endl;
    std::cerr << "  symbol              - Trading symbol (e.g., IBM)" << std::endl;
    std::cerr << "  side                - Order side: 'B' for Buy, 'S' for Sell" << std::endl;
    std::cerr << "  max_order_size      - Maximum order size (positive integer)" << std::endl;
    std::cerr << "  vwap_window_seconds - VWAP calculation window in seconds" << std::endl;
    std::cerr << "  market_data_ip      - Market data server IP address" << std::endl;
    std::cerr << "  market_data_port    - Market data server port" << std::endl;
    std::cerr << "  order_ip            - Order server IP address" << std::endl;
    std::cerr << "  order_port          - Order server port" << std::endl;
    std::cerr << "\nExample:" << std::endl;
    std::cerr << "  " << program_name << " IBM B 100 30 127.0.0.1 14000 127.0.0.1 15000" << std::endl;
}

bool parse_arguments(int argc, char* argv[], Config& config) {
    if (argc != 9) {
        std::cerr << "Error: Invalid number of arguments (expected 8, got " << (argc - 1) << ")" << std::endl;
        return false;
    }

    config.symbol = argv[1];
    if (config.symbol.empty() || config.symbol.length() > 8) {
        std::cerr << "Error: Symbol must be 1-8 characters" << std::endl;
        return false;
    }

    if (std::strlen(argv[2]) != 1 || (argv[2][0] != 'B' && argv[2][0] != 'S')) {
        std::cerr << "Error: Side must be 'B' or 'S'" << std::endl;
        return false;
    }
    config.side = argv[2][0];

    char* endptr;
    config.maxOrderSize = std::strtoul(argv[3], &endptr, 10);
    if (*endptr != '\0' || config.maxOrderSize == 0 || config.maxOrderSize > 1000000) {
        std::cerr << "Error: Max order size must be a positive integer (1-1000000)" << std::endl;
        return false;
    }

    config.vwapWindowSeconds = std::strtoul(argv[4], &endptr, 10);
    if (*endptr != '\0' || config.vwapWindowSeconds == 0 || config.vwapWindowSeconds > 3600) {
        std::cerr << "Error: VWAP window must be between 1 and 3600 seconds" << std::endl;
        return false;
    }

    config.marketDataHost = argv[5];
    if (config.marketDataHost.empty()) {
        std::cerr << "Error: Invalid market data IP address" << std::endl;
        return false;
    }

    config.marketDataPort = std::strtoul(argv[6], &endptr, 10);
    if (*endptr != '\0' || config.marketDataPort == 0 || config.marketDataPort > 65535) {
        std::cerr << "Error: Market data port must be between 1 and 65535" << std::endl;
        return false;
    }

    config.orderHost = argv[7];
    if (config.orderHost.empty()) {
        std::cerr << "Error: Invalid order server IP address" << std::endl;
        return false;
    }

    config.orderPort = std::strtoul(argv[8], &endptr, 10);
    if (*endptr != '\0' || config.orderPort == 0 || config.orderPort > 65535) {
        std::cerr << "Error: Order port must be between 1 and 65535" << std::endl;
        return false;
    }

    return true;
}

void print_config(const Config& config) {
    std::cout << "\n=== VWAP Trading System Configuration ===" << std::endl;
    std::cout << "Trading Parameters:" << std::endl;
    std::cout << "  Symbol: " << config.symbol << std::endl;
    std::cout << "  Side: " << config.side << " (" << (config.side == 'B' ? "BUY" : "SELL") << ")" << std::endl;
    std::cout << "  Max Order Size: " << config.maxOrderSize << std::endl;
    std::cout << "  VWAP Window: " << config.vwapWindowSeconds << " seconds" << std::endl;
    std::cout << "\nNetwork Configuration:" << std::endl;
    std::cout << "  Market Data: " << config.marketDataHost << ":" << config.marketDataPort << std::endl;
    std::cout << "  Order Server: " << config.orderHost << ":" << config.orderPort << std::endl;
    std::cout << "==========================================\n" << std::endl;
}

void print_startup_banner() {
    std::cout << "╔═══════════════════════════════════════╗" << std::endl;
    std::cout << "║    VWAP Trading System v1.0.0         ║" << std::endl;
    std::cout << "║    Press Ctrl+C to shutdown           ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════╝" << std::endl;
}

int main(int argc, char* argv[]) {
    Config config;
    if (!parse_arguments(argc, argv, config)) {
        print_usage(argv[0]);
        return 1;
    }

    const char* pinEnv = std::getenv("VWAP_PIN_CPU");
    if (pinEnv) {
        // macOS does not have sched_setaffinity; we can try to set QoS to user-interactive to reduce jitter
        #ifdef __APPLE__
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
        std::cout << "(QoS elevated for reduced jitter)" << std::endl;
        #endif
    }

    runtimeConfig().loadFromEnv();
    print_startup_banner();
    print_config(config);

    setup_signal_handlers();

    try {
        std::cout << "Initializing Order Manager..." << std::endl;
        OrderManager orderManager(
            config.symbol,
            config.side,
            config.maxOrderSize,
            config.vwapWindowSeconds
        );

        std::cout << "Initializing Network Manager..." << std::endl;
        NetworkManager networkManager;

        if (!networkManager.initialize(config)) {
            std::cerr << "Failed to initialize network connections" << std::endl;
            std::cerr << "Please check that the market data and order servers are running" << std::endl;
            return 1;
        }

        std::cout << "Network connections established successfully" << std::endl;

        uint64_t totalQuotes = 0;
        uint64_t totalTrades = 0;
        uint64_t totalOrders = 0;
        auto startTime = std::chrono::steady_clock::now();
        auto lastStatsTime = startTime;

        networkManager.setQuoteCallback([&](const QuoteMessage& quote) {
            if (std::strncmp(quote.symbol, config.symbol.c_str(),
                           std::min(sizeof(quote.symbol), config.symbol.length())) != 0) {
                return;
            }

            totalQuotes++;

            Optional<OrderMessage> orderOpt = orderManager.processQuote(quote);

            if (orderOpt.has_value()) {
                OrderMessage order = orderOpt.value();

                if (networkManager.sendOrder(order)) {
                    totalOrders++;

                    char symbolStr[9] = {0};
                    std::memcpy(symbolStr, order.symbol, 8);

                    std::cout << "[ORDER SENT] "
                              << (order.side == 'B' ? "BUY" : "SELL")
                              << " " << order.quantity
                              << " " << symbolStr
                              << " @ $" << std::fixed << std::setprecision(2)
                              << (order.price / 100.0)
                              << " (Order #" << totalOrders << ")"
                              << std::endl;
                } else {
                    std::cerr << "[ERROR] Failed to send order to server" << std::endl;
                }
            }
        });

        networkManager.setTradeCallback([&](const TradeMessage& trade) {
            if (std::strncmp(trade.symbol, config.symbol.c_str(),
                           std::min(sizeof(trade.symbol), config.symbol.length())) != 0) {
                return;
            }

            totalTrades++;

            orderManager.processTrade(trade);

            if (totalTrades % 10 == 0) {
                double currentVwap = orderManager.getCurrentVwap();
                if (currentVwap > 0) {
                    std::cout << "[VWAP UPDATE] Current VWAP: $"
                              << std::fixed << std::setprecision(2)
                              << (currentVwap / 100.0)
                              << " (after " << totalTrades << " trades)"
                              << std::endl;
                }
            }
        });

        std::cout << "\n=== Trading System Started ===" << std::endl;
        std::cout << "Waiting for market data..." << std::endl;
        std::cout << "System will be ready to trade after first VWAP window completes" << std::endl;

        while (!g_shutdown_requested) {
            networkManager.processEvents();

            auto now = std::chrono::steady_clock::now();
            auto timeSinceLastStats = std::chrono::duration_cast<std::chrono::seconds>(
                now - lastStatsTime).count();

            if (timeSinceLastStats >= 30) {
                lastStatsTime = now;

                auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                    now - startTime).count();

                std::cout << "\n[STATS] Uptime: " << uptime << "s"
                          << " | Quotes: " << totalQuotes
                          << " | Trades: " << totalTrades
                          << " | Orders: " << totalOrders;

                if (orderManager.isReadyToTrade()) {
                    std::cout << " | Status: READY";
                } else {
                    std::cout << " | Status: WAITING";
                }

                double currentVwap = orderManager.getCurrentVwap();
                if (currentVwap > 0) {
                    std::cout << " | VWAP: $" << std::fixed << std::setprecision(2)
                              << (currentVwap / 100.0);
                }

                std::cout << std::endl;
            }
        }

        std::cout << "\n=== Shutting Down ===" << std::endl;

        networkManager.stop();

        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();

        std::cout << "\n=== Final Statistics ===" << std::endl;
        std::cout << "Total Runtime: " << uptime << " seconds" << std::endl;
        std::cout << "Total Quotes Processed: " << totalQuotes << std::endl;
        std::cout << "Total Trades Processed: " << totalTrades << std::endl;
        std::cout << "Total Orders Sent: " << totalOrders << std::endl;

        if (totalQuotes > 0) {
            double orderRate = (100.0 * totalOrders) / totalQuotes;
            std::cout << "Order Rate: " << std::fixed << std::setprecision(2)
                      << orderRate << "%" << std::endl;
        }

        orderManager.printStatistics();

        if (totalOrders > 0) {
            orderManager.printOrderHistory(10);
        }

    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL ERROR] " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n[FATAL ERROR] Unknown exception occurred" << std::endl;
        return 1;
    }

    std::cout << "\nShutdown complete. Goodbye!" << std::endl;
    return 0;
}
