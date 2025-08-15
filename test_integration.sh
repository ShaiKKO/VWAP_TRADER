#!/bin/bash

echo "VWAP Trading System Integration Test"
echo "====================================="
echo ""

echo "Starting Market Data Simulator..."
./bin/market_simulator --port 9090 --symbol IBM --scenario steady --price 140.00 --rate 5 --duration 30 --verbose &
SIMULATOR_PID=$!
echo "Simulator PID: $SIMULATOR_PID"

sleep 2

echo ""
echo "Starting VWAP Trader..."
echo "Command: ./bin/vwap_trader IBM B 100 10 127.0.0.1 9090 127.0.0.1 9091"
./bin/vwap_trader IBM B 100 10 127.0.0.1 9090 127.0.0.1 9091 &
TRADER_PID=$!
echo "Trader PID: $TRADER_PID"

echo ""
echo "Running for 30 seconds..."
echo "Press Ctrl+C to stop early"

trap "echo 'Stopping...'; kill $SIMULATOR_PID $TRADER_PID 2>/dev/null; exit" INT TERM

wait $SIMULATOR_PID

echo ""
echo "Stopping trader..."
kill $TRADER_PID 2>/dev/null

echo ""
echo "Integration test complete!"
