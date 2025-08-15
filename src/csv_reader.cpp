#include "csv_reader.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>

CSVReader::CSVReader(const std::string& path) 
    : filePath(path), currentIndex(0), isLoaded(false),
      readBuffer(std::make_unique<char[]>(BUFFER_SIZE)) {
    records.reserve(INITIAL_RESERVE);
}

bool CSVReader::loadFile() {
    file.open(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open CSV file: " << filePath << std::endl;
        return false;
    }
    
    file.rdbuf()->pubsetbuf(readBuffer.get(), BUFFER_SIZE);
    
    std::string line;
    bool headerParsed = false;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        if (!headerParsed) {
            if (!parseHeader(line)) {
                std::cerr << "Failed to parse CSV header" << std::endl;
                return false;
            }
            headerParsed = true;
            continue;
        }
        
        MarketDataRecord record;
        if (parseLine(line, record)) {
            records.push_back(std::move(record));
        }
    }
    
    file.close();
    isLoaded = true;
    
    std::cout << "Loaded " << records.size() << " records from " << filePath << std::endl;
    return !records.empty();
}

const MarketDataRecord& CSVReader::next() noexcept {
    static MarketDataRecord empty;
    if (currentIndex < records.size()) {
        return records[currentIndex++];
    }
    return empty;
}

bool CSVReader::parseHeader(const std::string& line) {
    auto fields = splitCSV(line);
    return fields.size() >= 3;
}

bool CSVReader::parseLine(const std::string& line, MarketDataRecord& record) {
    auto fields = splitCSV(line);
    if (fields.size() < 3) return false;
    
    record.timestamp = parseTimestamp(fields[0]);
    
    if (fields[1] == "Q" || fields[1] == "QUOTE") {
        record.type = MarketDataRecord::QUOTE;
        if (fields.size() < 7) return false;
        
        record.symbol = fields[2];
        record.quote.bidPrice = parsePrice(fields[3]);
        record.quote.bidQuantity = parseQuantity(fields[4]);
        record.quote.askPrice = parsePrice(fields[5]);
        record.quote.askQuantity = parseQuantity(fields[6]);
    } else if (fields[1] == "T" || fields[1] == "TRADE") {
        record.type = MarketDataRecord::TRADE;
        if (fields.size() < 5) return false;
        
        record.symbol = fields[2];
        record.trade.price = parsePrice(fields[3]);
        record.trade.quantity = parseQuantity(fields[4]);
    } else {
        return false;
    }
    
    return true;
}

std::vector<std::string> CSVReader::splitCSV(const std::string& line) {
    std::vector<std::string> fields;
    fields.reserve(8);
    
    std::string field;
    bool inQuotes = false;
    
    for (char c : line) {
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            fields.push_back(std::move(field));
            field.clear();
        } else {
            field += c;
        }
    }
    
    if (!field.empty()) {
        fields.push_back(std::move(field));
    }
    
    for (auto& f : fields) {
        f.erase(0, f.find_first_not_of(" \t"));
        f.erase(f.find_last_not_of(" \t") + 1);
    }
    
    return fields;
}

uint64_t CSVReader::parseTimestamp(const std::string& timeStr) {
    if (timeStr.find(':') != std::string::npos) {
        int hours = 0, minutes = 0, seconds = 0, millis = 0;
        char sep;
        std::istringstream iss(timeStr);
        iss >> hours >> sep >> minutes >> sep >> seconds;
        
        if (timeStr.find('.') != std::string::npos) {
            iss >> sep >> millis;
        }
        
        uint64_t totalMillis = hours * 3600000ULL + minutes * 60000ULL + 
                               seconds * 1000ULL + millis;
        return totalMillis * 1000000ULL;
    } else {
        try {
            return std::stoull(timeStr);
        } catch (...) {
            return 0;
        }
    }
}

double CSVReader::parsePrice(const std::string& priceStr) {
    try {
        return std::stod(priceStr);
    } catch (...) {
        return 0.0;
    }
}

uint32_t CSVReader::parseQuantity(const std::string& qtyStr) {
    try {
        return static_cast<uint32_t>(std::stoul(qtyStr));
    } catch (...) {
        return 0;
    }
}

template<typename T>
T CSVReader::parseValue(const std::string& str, T defaultValue) const noexcept {
    std::istringstream iss(str);
    T value;
    if (iss >> value) {
        return value;
    }
    return defaultValue;
}

QuoteMessage CSVReader::convertToQuoteMessage(const MarketDataRecord& record) const {
    QuoteMessage msg;
    std::memset(&msg, 0, sizeof(msg));
    
    std::strncpy(msg.symbol, record.symbol.c_str(), 
                 std::min(sizeof(msg.symbol), record.symbol.length()));
    msg.timestamp = record.timestamp;
    msg.bidPrice = static_cast<int32_t>(record.quote.bidPrice * 100);
    msg.bidQuantity = record.quote.bidQuantity;
    msg.askPrice = static_cast<int32_t>(record.quote.askPrice * 100);
    msg.askQuantity = record.quote.askQuantity;
    
    return msg;
}

TradeMessage CSVReader::convertToTradeMessage(const MarketDataRecord& record) const {
    TradeMessage msg;
    std::memset(&msg, 0, sizeof(msg));
    
    msg.timestamp = record.timestamp;
    std::strncpy(msg.symbol, record.symbol.c_str(), 
                 std::min(sizeof(msg.symbol), record.symbol.length()));
    msg.quantity = record.trade.quantity;
    msg.price = static_cast<int32_t>(record.trade.price * 100);
    
    return msg;
}

CSVReplayEngine::CSVReplayEngine(std::unique_ptr<CSVReader> csvReader, double speed)
    : reader(std::move(csvReader)), baseTimestamp(0), replaySpeed(speed),
      isPaused(false), currentPosition(0) {
    if (reader && reader->size() > 0) {
        baseTimestamp = reader->getRecords()[0].timestamp;
    }
}

void CSVReplayEngine::start() noexcept {
    startTime = std::chrono::steady_clock::now();
    lastEmitTime = startTime;
    currentPosition = 0;
    isPaused = false;
}

bool CSVReplayEngine::getNextMessage(MarketDataRecord& record) {
    if (!reader || isPaused || currentPosition >= reader->size()) {
        return false;
    }
    
    const auto& nextRecord = reader->getRecords()[currentPosition];
    
    if (shouldEmitNow(nextRecord)) {
        record = nextRecord;
        currentPosition++;
        lastEmitTime = std::chrono::steady_clock::now();
        return true;
    }
    
    return false;
}

bool CSVReplayEngine::shouldEmitNow(const MarketDataRecord& record) const {
    if (replaySpeed <= 0) return true;
    
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastEmit = now - lastEmitTime;
    
    if (timeSinceLastEmit < MIN_INTERVAL) {
        return false;
    }
    
    uint64_t elapsedMicros = getElapsedMicros();
    uint64_t recordOffset = record.timestamp - baseTimestamp;
    uint64_t scaledOffset = static_cast<uint64_t>(recordOffset / replaySpeed);
    
    return elapsedMicros >= scaledOffset;
}

uint64_t CSVReplayEngine::getElapsedMicros() const noexcept {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - startTime;
    return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
}

uint64_t CSVReplayEngine::getReplayTimestamp(uint64_t originalTimestamp) const noexcept {
    uint64_t offset = originalTimestamp - baseTimestamp;
    return baseTimestamp + static_cast<uint64_t>(offset / replaySpeed);
}