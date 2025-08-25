#ifndef CSV_READER_H
#define CSV_READER_H

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <utility>
#include "message.h"

struct MarketDataRecord {
    enum Type { QUOTE, TRADE };

    Type type;
    uint64_t timestamp;
    std::string symbol;

    struct QuoteData {
        double bidPrice;
        uint32_t bidQuantity;
        double askPrice;
        uint32_t askQuantity;
    };

    struct TradeData {
        double price;
        uint32_t quantity;
    };

    union {
        QuoteData quote;
        TradeData trade;
    };

    MarketDataRecord() noexcept : type(QUOTE), timestamp(0) {
        quote = {0.0, 0, 0.0, 0};
    }

    MarketDataRecord(const MarketDataRecord& other)
        : type(other.type), timestamp(other.timestamp), symbol(other.symbol) {
        if (type == QUOTE) {
            quote = other.quote;
        } else {
            trade = other.trade;
        }
    }

    MarketDataRecord(MarketDataRecord&& other) noexcept
        : type(other.type), timestamp(other.timestamp), symbol(std::move(other.symbol)) {
        if (type == QUOTE) {
            quote = other.quote;
        } else {
            trade = other.trade;
        }
    }

    MarketDataRecord& operator=(const MarketDataRecord& other) {
        if (this != &other) {
            type = other.type;
            timestamp = other.timestamp;
            symbol = other.symbol;
            if (type == QUOTE) {
                quote = other.quote;
            } else {
                trade = other.trade;
            }
        }
        return *this;
    }

    MarketDataRecord& operator=(MarketDataRecord&& other) noexcept {
        if (this != &other) {
            type = other.type;
            timestamp = other.timestamp;
            symbol = std::move(other.symbol);
            if (type == QUOTE) {
                quote = other.quote;
            } else {
                trade = other.trade;
            }
        }
        return *this;
    }
};

class CSVReader final {
private:
    std::string filePath;
    std::ifstream file;
    std::vector<MarketDataRecord> records;
    size_t currentIndex;
    bool isLoaded;

    static constexpr size_t BUFFER_SIZE = 65536;
    std::unique_ptr<char[]> readBuffer;

    static constexpr size_t INITIAL_RESERVE = 10000;

public:
    explicit CSVReader(const std::string& path);
    ~CSVReader() = default;

    CSVReader(const CSVReader&) = delete;
    CSVReader& operator=(const CSVReader&) = delete;

    CSVReader(CSVReader&&) = default;
    CSVReader& operator=(CSVReader&&) = default;

    bool loadFile();
    bool hasNext() const noexcept { return currentIndex < records.size(); }
    const MarketDataRecord& next() noexcept;
    void reset() noexcept { currentIndex = 0; }
    size_t size() const noexcept { return records.size(); }

    const std::vector<MarketDataRecord>& getRecords() const noexcept { return records; }

    QuoteMessage convertToQuoteMessage(const MarketDataRecord& record) const;
    TradeMessage convertToTradeMessage(const MarketDataRecord& record) const;

private:
    bool parseHeader(const std::string& line);
    bool parseLine(const std::string& line, MarketDataRecord& record);
    std::vector<std::string> splitCSV(const std::string& line);
    uint64_t parseTimestamp(const std::string& timeStr);
    double parsePrice(const std::string& priceStr);
    uint32_t parseQuantity(const std::string& qtyStr);

    template<typename T>
    T parseValue(const std::string& str, T defaultValue) const noexcept;
};

class CSVReplayEngine final {
private:
    std::unique_ptr<CSVReader> reader;
    std::chrono::steady_clock::time_point startTime;
    uint64_t baseTimestamp;
    double replaySpeed;
    bool isPaused;
    size_t currentPosition;

    mutable std::chrono::steady_clock::time_point lastEmitTime;
    static constexpr auto MIN_INTERVAL = std::chrono::microseconds(100);

public:
    explicit CSVReplayEngine(std::unique_ptr<CSVReader> csvReader, double speed = 1.0);
    ~CSVReplayEngine() = default;

    CSVReplayEngine(const CSVReplayEngine&) = delete;
    CSVReplayEngine& operator=(const CSVReplayEngine&) = delete;

    void start() noexcept;
    void pause() noexcept { isPaused = true; }
    void resume() noexcept { isPaused = false; }
    void setSpeed(double speed) noexcept { replaySpeed = speed > 0 ? speed : 1.0; }

    bool getNextMessage(MarketDataRecord& record);
    bool shouldEmitNow(const MarketDataRecord& record) const;

    size_t getPosition() const noexcept { return currentPosition; }
    size_t getTotalRecords() const noexcept { return reader ? reader->size() : 0; }
    double getProgress() const noexcept {
        return reader && reader->size() > 0
            ? static_cast<double>(currentPosition) / reader->size() * 100.0
            : 0.0;
    }

private:
    uint64_t getElapsedMicros() const noexcept;
    uint64_t getReplayTimestamp(uint64_t originalTimestamp) const noexcept;
};

#endif
