#!/usr/bin/env python3
"""Simple order server for testing VWAP trader"""

import socket
import struct
import sys
import time
from datetime import datetime

def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9091

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('127.0.0.1', port))
    server_socket.listen(5)

    print(f"Order server listening on port {port}")

    try:
        while True:
            client_socket, addr = server_socket.accept()
            print(f"Order client connected from {addr}")

            try:
                while True:
                    header_data = client_socket.recv(5)
                    if not header_data:
                        break

                    if len(header_data) == 5:
                        length, msg_type = struct.unpack('<IB', header_data)
                        print(f"  Received order header: length={length}, type={msg_type}")

                        body_data = client_socket.recv(length)
                        if len(body_data) >= 24:  # Minimum order message size
                            symbol = body_data[:8].decode('ascii', errors='ignore').rstrip('\x00')
                            timestamp, side, quantity, price = struct.unpack('<QBIi', body_data[8:25])

                            price_dollars = price / 100.0
                            side_str = "BUY" if side == 1 else "SELL"

                            print(f"  ORDER: {symbol} {side_str} {quantity} @ ${price_dollars:.2f}")
                            print(f"         Timestamp: {timestamp}")

            except Exception as e:
                print(f"Client error: {e}")
            finally:
                client_socket.close()
                print(f"Order client disconnected")

    except KeyboardInterrupt:
        print("\nShutting down order server")
    finally:
        server_socket.close()

if __name__ == "__main__":
    main()
