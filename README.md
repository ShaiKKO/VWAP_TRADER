# VWAP Trading System

## Overview
A high-performance Volume-Weighted Average Price (VWAP) trading system built with C++11/14, featuring real-time market data processing, sliding window VWAP calculation, and automated trading logic.

## Key Features
- **Staff/Principal Engineer Quality**: Production-ready code with comprehensive error handling
- **Thread Safety**: Mutex-protected shared resources, thread-local RNG
- **Partial I/O Handling**: Complete support for partial sends with offset tracking
- **Protocol Compliance**: Strict adherence to wire format specification
- **Memory Safety**: Smart pointers (unique_ptr) throughout, no raw new/delete
- **Performance Optimized**: Cache-line aligned metrics, lock-free where possible
- **Robust Networking**: Exponential backoff with jitter for reconnections
- **Overflow Protection**: Saturation arithmetic in VWAP calculations

## Protocol Specification

### Wire Format
All messages use little-endian byte ordering and are transmitted in a binary format with a 2-byte header followed by the message body.

### Message Header (2 bytes)
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | length | Message body length in bytes |
| 1 | 1 | type | Message type (1=Quote, 2=Trade, 3=Order) |

### Quote Message (32 bytes on wire)
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 8 | symbol | Stock symbol (null-padded ASCII) |
| 8 | 8 | timestamp | Unix timestamp in nanoseconds |
| 16 | 4 | bidQuantity | Bid quantity (unsigned) |
| 20 | 4 | bidPrice | Bid price in cents (signed) |
| 24 | 4 | askQuantity | Ask quantity (unsigned) |
| 28 | 4 | askPrice | Ask price in cents (signed) |

### Trade Message (24 bytes on wire)
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 8 | symbol | Stock symbol (null-padded ASCII) |
| 8 | 8 | timestamp | Unix timestamp in nanoseconds |
| 16 | 4 | quantity | Trade quantity (unsigned) |
| 20 | 4 | price | Trade price in cents (signed) |

### Order Message (25 bytes on wire)
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 8 | symbol | Stock symbol (null-padded ASCII) |
| 8 | 8 | timestamp | Unix timestamp in nanoseconds |
| 16 | 1 | side | Order side ('B'=Buy, 'S'=Sell) |
| 17 | 4 | quantity | Order quantity (unsigned, **UNALIGNED**) |
| 21 | 4 | price | Order price in cents (signed) |

**Note**: The Order message has an unaligned field at offset 17, which requires special handling during serialization/deserialization.

## Native vs Wire Format Rationale

Due to alignment requirements in C++, native structs differ from wire format:
- **QuoteMessage**: 32 bytes both native and wire (naturally aligned)
- **TradeMessage**: 24 bytes both native and wire (naturally aligned)
- **OrderMessage**: 28 bytes native (with padding), 25 bytes wire (packed)

The system uses separate serialization/deserialization functions to convert between native structs and wire format, ensuring:
1. **Correct alignment**: Native structs maintain natural alignment for performance
2. **Wire compatibility**: Exact byte-level protocol compliance
3. **Endianness handling**: Automatic conversion between host and network byte order
4. **Unaligned access safety**: No undefined behavior from misaligned memory access

This separation allows the code to be both performant (aligned native structs) and correct (exact wire format).

## Protocol Version and Deviations

**Protocol Version**: 1.0

### Key Implementation Decisions:
1. **2-byte header**: Removed any reserved/padding bytes from header per spec
2. **8-byte symbols**: All message types use 8-byte symbols (not 4-byte)
3. **Zero values allowed**: Quantities and prices of 0 are valid per spec
4. **Little-endian**: All multi-byte fields use little-endian encoding
5. **Unaligned Order.quantity**: Special handling for the unaligned field at offset 17

### Known Deviations from Original Spec:
- None. The implementation strictly follows the binary protocol specification.

## Building and Running

### Prerequisites
- C++11/14 compatible compiler
- CMake 3.10 or higher
- POSIX-compliant system (Linux/macOS)

### Build Instructions
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Running the System
```bash
# 1. Start the market data simulator (port 9090 example)
./simulator --port 9090 --symbol IBM --rate 20 --scenario steady

# 2. Start the simple Python order server (expects raw 25-byte order messages, no header)
./simple_order_server.py 15000

# 3. Run the VWAP trading system (symbol side maxOrder window md_host md_port order_host order_port)
./bin/vwap_trader IBM B 500 30 127.0.0.1 9090 127.0.0.1 15000
```

If you omit arguments you will see usage guidance. Ensure the order server port matches the last two arguments supplied to the trader. The legacy example using port 9091 has been updated to 15000 to match the spec examples.

## Architecture

### Core Components
- **Message Parser/Serializer**: Field-by-field conversion with endian handling
- **Network Manager**: Connection management with exponential backoff
- **VWAP Calculator**: Sliding window with overflow protection
- **Order Client**: Partial send handling with queue management
- **Simulator**: Thread-safe market data generation

### Thread Safety
- Mutex protection for shared client socket vectors
- Thread-local RNG with atomic seed counter
- Lock-free memory pools where applicable
- Cache-line aligned metrics to prevent false sharing

### Error Handling
- EINTR retry logic for system calls
- EPIPE/ECONNRESET detection and recovery
- Partial send buffering with offset tracking
- Queue depth limits with overflow policy

## Performance Considerations

### Optimizations
- Cache-line aligned hot/cold metric separation
- Branch prediction hints (__builtin_expect)
- Zero-copy techniques where possible
- Batched expiry processing in VWAP calculator

### Metrics
The system tracks comprehensive metrics including:
- Messages sent/received
- Bytes transferred
- Connection statistics
- Queue high-water marks
- Overflow/rejection counters
- Resynchronization events during header scanning (resyncEvents)
- Per-message decision latency (min/avg/max via nanosecond counters)

Latency sampling measures wall-clock decision latency using std::steady_clock from quote receipt into DecisionEngine through order creation (min/avg/max). Feed timestamps are still recorded for sequencing but not used for latency math.

Resynchronization events are counted when the parser skips over a bounded number of bytes (up to 256) searching for a plausible header after encountering malformed data. A spike indicates upstream corruption or desynchronization.

Overflow handling policy:
1. VWAP accumulation uses checked arithmetic; on potential 64-bit overflow the trade is rejected and counted as dropped.
2. Send queue enforces MAX_QUEUE_DEPTH; excess orders are dropped with messagesDropped incremented.
3. Receive buffer compaction occurs before declaring overflow; if still full the buffer is cleared and messagesDropped increments.
4. Parser invalid headers trigger bounded resync; skipped bytes increment resyncEvents.

## Testing

Run the test suite:
```bash
make test
```

Key test areas:
- Wire format compliance
- Partial send scenarios
- Overflow handling
- Malformed message rejection
- Thread safety verification
- VWAP window boundary correctness and overflow rejection
- Parser round-trip header/body validation
- Decision engine order triggering edge cases

Optional Fuzzing Stub: A lightweight harness (see `tests/` placeholder if added) can feed random byte streams into `MessageBuffer::append` + `extractMessage` to stress header resynchronization and ensure no out-of-bounds or UB occurs. This can be extended with mutation strategies (bit flips around header boundary) for deeper protocol robustness.