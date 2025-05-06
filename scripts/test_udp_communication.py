
#!/usr/bin/env python3
import socket
import argparse
import time
import sys
import threading
import traceback
import struct

# Message types from SharedMemorySyncMessageType enum
SYNC_MSG_TYPES = {
    1: "INIT",    # Initial sync request/full memory state
    2: "UPDATE",  # Incremental memory update
    3: "ACK",     # Acknowledgment
    4: "HEARTBEAT" # Heartbeat message
}

def decode_sync_message(data):
    """Decode EtherRecorder shared memory sync message"""
    if len(data) < 4:  # Minimum size for header
        return "Invalid message (too small, len={})".format(len(data))
    
    try:
        # Extract basic header fields
        msg_type = data[0]
        seq_num = struct.unpack("<H", data[1:3])[0]
        name_len = data[3]
        
        # Debug output for troubleshooting
        debug_info = f"Raw header: type={msg_type}, seq={seq_num}, name_len={name_len}"
        debug_info += f"\nRaw bytes: {' '.join(f'{b:02x}' for b in data[:20])}..."
        
        if name_len > 64:
            return f"Invalid name length: {name_len}\n{debug_info}"
        
        if len(data) < 4 + name_len:
            return f"Message too short for name (need {4+name_len}, got {len(data)})\n{debug_info}"
        
        # Extract block name
        try:
            block_name = data[4:4+name_len].decode('utf-8')
        except UnicodeDecodeError:
            block_name = "DECODE_ERROR"
            debug_info += f"\nName decode error: {' '.join(f'{b:02x}' for b in data[4:4+name_len])}"
        
        # Calculate positions for offset and length fields
        offset_pos = 4 + 64  # Fixed size block name field (4 bytes header + 64 bytes name)
        
        if len(data) < offset_pos + 8:
            return f"Message too short for offset/length (need {offset_pos+8}, got {len(data)})\n{debug_info}"
        
        # Extract offset and data length
        try:
            offset = struct.unpack("<I", data[offset_pos:offset_pos+4])[0]
            data_len = struct.unpack("<I", data[offset_pos+4:offset_pos+8])[0]
        except struct.error as e:
            return f"Error unpacking offset/length: {e}\n{debug_info}"
        
        # Extract actual data
        data_pos = offset_pos + 8
        if len(data) < data_pos + data_len:
            return f"Data truncated (need {data_pos+data_len}, got {len(data)})\n{debug_info}"
        
        actual_data = data[data_pos:data_pos+data_len]
        
        # Format the decoded message
        type_str = SYNC_MSG_TYPES.get(msg_type, f"UNKNOWN({msg_type})")
        result = f"SyncMsg: {type_str}, Seq: {seq_num}, Block: '{block_name}', Offset: {offset}, Len: {data_len}"
        
        # For small data amounts, show the actual data
        if data_len <= 32:
            hex_data = ' '.join(f'{b:02x}' for b in actual_data)
            result += f"\nData: {hex_data}"
            
            # Try to interpret as text if possible
            try:
                text_data = actual_data.decode('utf-8')
                printable = all(32 <= ord(c) < 127 or c in '\r\n\t' for c in text_data)
                if printable:
                    result += f"\nText: {text_data}"
            except UnicodeDecodeError:
                pass
        else:
            result += f"\nData: {data_len} bytes (first 16 shown)"
            hex_data = ' '.join(f'{b:02x}' for b in actual_data[:16])
            result += f"\n{hex_data}..."
            
        return result
    except Exception as e:
        # Add detailed error information and hex dump of the message
        error_info = f"Error decoding sync message: {str(e)}\n{traceback.format_exc()}"
        error_info += "\nMessage hex dump:"
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_line = ' '.join(f'{b:02x}' for b in chunk)
            ascii_line = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
            error_info += f"\n{i:04x}: {hex_line:<48} {ascii_line}"
        return error_info

def setup_udp_socket(bind_addr='0.0.0.0', bind_port=0):
    """Create and configure a UDP socket"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((bind_addr, bind_port))
    
    # Get the actual port if we used 0 (auto-assign)
    actual_port = sock.getsockname()[1]
    print(f"UDP socket bound to {bind_addr}:{actual_port}")
    
    return sock, actual_port

def receive_thread_func(sock, stop_event):
    """Thread function to receive and display UDP packets"""
    print("Receiver thread started")
    sock.settimeout(0.5)  # Set timeout to allow checking stop_event
    
    while not stop_event.is_set():
        try:
            data, addr = sock.recvfrom(4096)
            timestamp = time.strftime("%H:%M:%S")
            
            # Print raw hex data for debugging
            print(f"\n[{timestamp}] Received {len(data)} bytes from {addr[0]}:{addr[1]}")
            print(f"Raw data (first 32 bytes): {' '.join(f'{b:02x}' for b in data[:32])}")
            
            # First byte is message type in our protocol
            if len(data) > 0 and data[0] in SYNC_MSG_TYPES:
                # This looks like one of our sync messages
                decoded = decode_sync_message(data)
                print(f"\n[{timestamp}] Decoded message from {addr[0]}:{addr[1]}:\n{decoded}")
            else:
                try:
                    # Try to decode as UTF-8 text
                    decoded = data.decode('utf-8')
                    print(f"\n[{timestamp}] Received text from {addr[0]}:{addr[1]}: {decoded}")
                except UnicodeDecodeError:
                    # If not valid UTF-8, show as hex
                    hex_data = ' '.join(f'{b:02x}' for b in data)
                    print(f"\n[{timestamp}] Received binary from {addr[0]}:{addr[1]}: {hex_data}")
                    
                    # Also show as ASCII if possible
                    ascii_repr = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data)
                    print(f"ASCII representation: {ascii_repr}")
                
        except socket.timeout:
            # This is expected due to the timeout we set
            pass
        except Exception as e:
            print(f"Error in receiver thread: {e}")
            traceback.print_exc()
            break
    
    print("Receiver thread stopped")

def main():
    parser = argparse.ArgumentParser(description='Test UDP communication')
    parser.add_argument('--listen-port', type=int, default=0,
                        help='Port to listen on (0 for auto-assign)')
    parser.add_argument('--target-host', default='localhost',
                        help='Target host to send to')
    parser.add_argument('--target-port', type=int, 
                        help='Target port to send to (required for sending messages)')
    parser.add_argument('--interval', type=float, default=1.0,
                        help='Interval between automatic messages (seconds)')
    args = parser.parse_args()
    
    try:
        # Setup UDP socket
        sock, actual_listen_port = setup_udp_socket(bind_port=args.listen_port)
        
        # Start receiver thread
        stop_event = threading.Event()
        receiver = threading.Thread(target=receive_thread_func, args=(sock, stop_event))
        receiver.daemon = True
        receiver.start()
        
        # Show listen information
        print(f"Listening on port {actual_listen_port}")
        
        # Show target information if provided
        if args.target_port:
            print(f"Target: {args.target_host}:{args.target_port}")
            print("Commands:")
            print("  send <message>  - Send a text message")
            print("  hex <hex data>  - Send hex data (space-separated bytes, e.g. '01 02 03')")
            print("  auto <message>  - Send message repeatedly at set interval")
            print("  stop            - Stop auto-sending")
            print("  quit            - Exit the program")
        else:
            print("No target port specified - running in listen-only mode")
            print("Use Ctrl+C to exit")
        
        auto_send = False
        auto_message = None
        last_send_time = 0
        
        while True:
            # Handle auto-sending if target port is specified
            if args.target_port and auto_send and time.time() - last_send_time >= args.interval:
                sock.sendto(auto_message.encode('utf-8'), (args.target_host, args.target_port))
                print(f"Auto-sent: {auto_message}")
                last_send_time = time.time()
            
            # Check for user input (non-blocking) if target port is specified
            if args.target_port and sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                line = input().strip()
                
                if line.lower() == 'quit':
                    break
                    
                elif line.lower() == 'stop':
                    auto_send = False
                    print("Auto-send stopped")
                    
                elif line.startswith('send '):
                    message = line[5:]
                    sock.sendto(message.encode('utf-8'), (args.target_host, args.target_port))
                    print(f"Sent: {message}")
                    
                elif line.startswith('hex '):
                    try:
                        hex_str = line[4:].strip()
                        hex_bytes = bytes.fromhex(hex_str.replace(' ', ''))
                        sock.sendto(hex_bytes, (args.target_host, args.target_port))
                        print(f"Sent hex: {' '.join(f'{b:02x}' for b in hex_bytes)}")
                    except ValueError as e:
                        print(f"Invalid hex format: {e}")
                        
                elif line.startswith('auto '):
                    auto_message = line[5:]
                    auto_send = True
                    last_send_time = 0  # Send immediately
                    print(f"Auto-sending '{auto_message}' every {args.interval} seconds")
                    
                else:
                    print("Unknown command")
            
            # If no target port, just wait for incoming messages
            elif not args.target_port:
                time.sleep(0.5)  # Longer sleep in listen-only mode
                continue
            
            # Small sleep to prevent CPU hogging
            time.sleep(0.1)
            
    except KeyboardInterrupt:
        print("\nExiting...")
    except Exception as e:
        print(f"Error: {e}")
        traceback.print_exc()
    finally:
        # Clean up
        if 'stop_event' in locals():
            stop_event.set()
        if 'receiver' in locals() and receiver.is_alive():
            receiver.join(timeout=2.0)
        if 'sock' in locals():
            sock.close()

if __name__ == "__main__":
    # Import select here to avoid issues on Windows
    import select
    main()







