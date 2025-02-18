import socket
import random
import sys

def udp_proxy(drop_rate):
    # Hardcoded values
    listen_ip = "127.0.0.1"  # localhost
    listen_port = 8080       # Listen on port 8080
    forward_ip = "127.0.0.1" # Forward to localhost
    forward_port = 8081      # Forward to port 8081

    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Bind the socket to the listen IP and port
    sock.bind((listen_ip, listen_port))
    
    print(f"Proxy listening on {listen_ip}:{listen_port}")
    print(f"Forwarding to {forward_ip}:{forward_port} with a packet drop rate of {drop_rate * 100}%")
    
    # Variables to track the client address and packet counts
    client_addr = None
    client_to_server_count = 0  # Packets from client to server
    server_to_client_count = 0  # Packets from server to client

    while True:
        # Receive data from either the client or server
        data, addr = sock.recvfrom(65535)  # Maximum UDP packet size
        
        if client_addr is None:
            # First packet: assume it's from the client
            client_addr = addr
            print(f"Client address set to {client_addr}")

        if addr == client_addr:
            # Packet is from the client (client-to-server)
            client_to_server_count += 1
            if client_to_server_count <= 3:
                print(f"Forwarded initial packet {client_to_server_count} from client {addr} to server")
                sock.sendto(data, (forward_ip, forward_port))
            else:
                # Apply drop rate after the first 3 packets
                if random.random() < drop_rate:
                    print(f"Dropped packet from client {addr}")
                else:
                    sock.sendto(data, (forward_ip, forward_port))
                    print(f"Forwarded packet from client {addr} to server")
        else:
            # Packet is from the server (server-to-client)
            server_to_client_count += 1
            if server_to_client_count <= 3:
                print(f"Forwarded initial packet {server_to_client_count} from server {addr} to client")
                sock.sendto(data, client_addr)  # Send back to the client
            else:
                # Apply drop rate after the first 3 packets
                if random.random() < drop_rate:
                    print(f"Dropped packet from server {addr}")
                else:
                    sock.sendto(data, client_addr)
                    print(f"Forwarded packet from server {addr} to client")

if __name__ == "__main__":
    # Set the drop rate (e.g., 0.2 for 20% packet loss)
    drop_rate = 0.2  # Change this value as needed

    if drop_rate < 0 or drop_rate > 1:
        print("Drop rate must be between 0 and 1")
        sys.exit(1)
    
    udp_proxy(drop_rate)