import socket

def main():
    server_ip = input("Enter server IP address: ")
    server_port = 8080
    buffer_size = 4096

    source_dir = input("Enter source directory path: ")
    destination = input("Enter destination file path (including filename, without extension): ")
    compression_level = input("Enter compression level (0-9): ")
    password = input("Enter password (or leave blank for no password): ")

    # cream socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        # conectam
        client_socket.connect((server_ip, server_port))
        print("Connected to server")

        # request
        message = f"{source_dir} {destination} {compression_level} {password}"
        client_socket.sendall(message.encode())

        # primim
        response = client_socket.recv(buffer_size).decode()
        print(f"Server: {response}")

    except Exception as e:
        print(f"Error: {e}")
    finally:
        # inchidem socket
        client_socket.close()

if __name__ == "__main__":
    main()
