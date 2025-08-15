#!/bin/bash

# Integration test script for VWAP Trading System
# This script starts the market simulator and then connects the VWAP trader

echo "VWAP Trading System Integration Test"
echo "====================================="
echo ""

# Start the simulator in the background
echo "Starting Market Data Simulator..."
./bin/market_simulator --port 9090 --symbol IBM --scenario steady --price 140.00 --rate 5 --duration 30 --verbose &
SIMULATOR_PID=$!
echo "Simulator PID: $SIMULATOR_PID"

# Wait for simulator to start
sleep 2

# Start the VWAP trader
echo ""
echo "Starting VWAP Trader..."
echo "Command: ./bin/vwap_trader IBM B 100 10 127.0.0.1 9090 127.0.0.1 9091"
./bin/vwap_trader IBM B 100 10 127.0.0.1 9090 127.0.0.1 9091 &
TRADER_PID=$!
echo "Trader PID: $TRADER_PID"

# Wait for both to run
echo ""
echo "Running for 30 seconds..."
echo "Press Ctrl+C to stop early"

# Trap Ctrl+C to clean up
trap "echo 'Stopping...'; kill $SIMULATOR_PID $TRADER_PID 2>/dev/null; exit" INT TERM

# Wait for simulator to finish (30 seconds)
wait $SIMULATOR_PID

# Stop trader
echo ""
echo "Stopping trader..."
kill $TRADER_PID 2>/dev/null

echo ""
echo "Integration test complete!"