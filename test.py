import subprocess
import time
import os
import signal

CLIENT_PORT = 8080
SERVER_PORT = 8081

# Path to the project folder
project_folder = "./project"

# Paths to the server and client binaries
server_binary = os.path.join(project_folder, "server")
client_binary = os.path.join(project_folder, "client")

# Function to run a command in the project directory
def run_command_in_project(cmd):
    print(f"Running command: {cmd}")
    process = subprocess.Popen(cmd, shell=True, cwd=project_folder)
    process.wait()
    if process.returncode != 0:
        print(f"Command failed with exit code {process.returncode}")
        exit(1)

# Run `make` to build the binaries
run_command_in_project("make")

# Function to run a test
def run_test(server_cmd, client_cmd, expected_out_size, expected_in_size, test_name):
    print(f"Running test: {test_name}")

    # Start the server process
    server_process = subprocess.Popen(server_cmd, shell=True, preexec_fn=os.setsid)

    # Wait a short moment to ensure the server is up
    time.sleep(1)

    # Start the client process
    client_process = subprocess.Popen(client_cmd, shell=True, preexec_fn=os.setsid)

    # Let both processes run for a bit
    time.sleep(5)

    # Kill both processes
    os.killpg(os.getpgid(server_process.pid), signal.SIGKILL)
    os.killpg(os.getpgid(client_process.pid), signal.SIGKILL)

    # Wait for the processes to terminate
    server_process.wait()
    client_process.wait()

    # Function to get the size of a file using `wc -c`
    def get_file_size(filename):
        result = subprocess.run(['wc', '-c', filename], stdout=subprocess.PIPE)
        size = int(result.stdout.split()[0])
        return size

    # Check the sizes of the output files
    out_size = get_file_size('out.out')
    in_size = get_file_size('in.out')

    # Verify the sizes
    if out_size == expected_out_size and in_size == expected_in_size:
        print(f"Test '{test_name}' passed: File sizes match expected values.")
    else:
        print(f"Test '{test_name}' failed: out.out size is {out_size} (expected {expected_out_size}), in.out size is {in_size} (expected {expected_in_size}).")
    print()

# Test 1: Server sends to client
server_cmd_1 = f"head -c 6969 /dev/urandom | {server_binary} {SERVER_PORT} > out.out"
client_cmd_1 = f"{client_binary} localhost {CLIENT_PORT} > in.out"
run_test(server_cmd_1, client_cmd_1, 0, 6969, "Server sends to client")

# Test 2: Client sends to server
server_cmd_2 = f"{server_binary} {SERVER_PORT} > out.out"
client_cmd_2 = f"head -c 9696 /dev/urandom | {client_binary} localhost {CLIENT_PORT} > in.out"
run_test(server_cmd_2, client_cmd_2, 9696, 0, "Client sends to server")

# Test 3: Both send to each other
server_cmd_3 = f"head -c 6969 /dev/urandom | {server_binary} {SERVER_PORT} > out.out"
client_cmd_3 = f"head -c 9696 /dev/urandom | {client_binary} localhost {CLIENT_PORT} > in.out"
run_test(server_cmd_3, client_cmd_3, 9696, 6969, "Both send to each other")

run_command_in_project('make clean')