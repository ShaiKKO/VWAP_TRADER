#!/usr/bin/env python3
"""Simple order server for VWAP Trading System.

Receives fixed 25-byte order messages (no header) per protocol spec.
"""

import socket
import struct
import sys
from datetime import datetime

ORDER_SIZE = 25  # Fixed size per spec

def recv_fully(sock, n):
    data = bytearray()
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            return None
        data.extend(chunk)
    return bytes(data)

def parse_order(payload):
    if len(payload) != ORDER_SIZE:
        raise ValueError("Invalid order size")
    symbol_raw = payload[0:8]
    timestamp, = struct.unpack_from('<Q', payload, 8)
    side = chr(payload[16])
    quantity, price = struct.unpack_from('<Ii', payload, 17)
    symbol = symbol_raw.decode('ascii', errors='ignore').rstrip('\x00')
    return {
        'symbol': symbol,
        'timestamp': timestamp,
        'side': side,
        'quantity': quantity,
        'price': price,
    }

def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 15000

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('127.0.0.1', port))
    server_socket.listen(16)

    print(f"Order server listening on 127.0.0.1:{port} (expecting {ORDER_SIZE}-byte orders)")

    try:
        while True:
            client_socket, addr = server_socket.accept()
            print(f"Order client connected from {addr}")
            try:
                while True:
                    payload = recv_fully(client_socket, ORDER_SIZE)
                    if payload is None:
                        break
                    try:
                        order = parse_order(payload)
                    except Exception as e:
                        print(f"Malformed order ({e}); closing connection")
                        break
                    side_str = 'BUY' if order['side'] == 'B' else 'SELL'
                    price_dollars = order['price'] / 100.0
                    ts = order['timestamp']
                    print(f"  ORDER {side_str} {order['quantity']:>6} {order['symbol']:<8} @ ${price_dollars:>.2f} ts={ts}")
            except Exception as e:
                print(f"Client error: {e}")
            finally:
                client_socket.close()
                print("Order client disconnected")
    except KeyboardInterrupt:
        print("\nShutting down order server")
    finally:
        server_socket.close()

if __name__ == '__main__':
    main()