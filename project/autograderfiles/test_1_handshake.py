import unittest
import os
import socket
import fcntl
import time

import test_0_compilation

from gradescope_utils.autograder_utils.decorators import number, hide_errors, partial_credit
from utils import ProcessRunner


start_port = 7080


class TestHandshake(unittest.TestCase):
    def make_test(self, name, use_ref_server):
        timeout = 0.25

        global start_port
        start_port += 1
        server_port = start_port
        start_port += 1
        client_port = start_port

        # Find dir
        paths_to_check = [
            "/autograder/submission/project/Makefile",
            "/autograder/submission/Makefile"
        ]

        makefile_dir = None
        for path in paths_to_check:
            if os.path.isfile(path):
                makefile_dir = os.path.dirname(path)
                break

        if makefile_dir is None:
            print("Makefile not found. Verify your submission has the correct files.")
            self.fail()

        file = b''

        r_ref = "/autograder/source/src"
        if use_ref_server:
            server_runner = ProcessRunner(
                f'{r_ref}/server {server_port}', file, name + "_refserver.out")
            client_runner = ProcessRunner(
                f'{makefile_dir}/client localhost {client_port}', file, name + "_yourclient.out")
        else:
            server_runner = ProcessRunner(
                f'{makefile_dir}/server {server_port}', file, name + "_yourserver.out")
            client_runner = ProcessRunner(
                f'{r_ref}/client localhost {client_port}', file, name + "_refclient.out")

        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        c = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        fcntl.fcntl(s, fcntl.F_SETFL, os.O_NONBLOCK)
        fcntl.fcntl(c, fcntl.F_SETFL, os.O_NONBLOCK)
        c.bind(('localhost', client_port))

        server_runner.run()
        time.sleep(0.05)
        client_runner.run()

        start_time = time.time()
        server_packets = 0
        client_packets = 0
        s_packets = []
        c_packets = []

        c_addr = None

        server_stdout = b''
        client_stdout = b''
        while time.time() - start_time < timeout:
            if ((server_runner.process and server_runner.process.poll()) or
                    (client_runner.process and client_runner.process.poll())):
                break

            try:
                packet, _ = s.recvfrom(2000)
                if len(packet) > 11 and packet[:4] not in s_packets:
                    s_packets.append(packet[:4])
                    server_packets += 1
                if c_addr:
                    c.sendto(packet, c_addr)
            except BlockingIOError:
                pass

            try:
                packet, c_addr = c.recvfrom(2000)
                if len(packet) > 11 and packet[:4] not in c_packets:
                    c_packets.append(packet[:4])
                    client_packets += 1
                s.sendto(packet, ('localhost', server_port))
            except BlockingIOError:
                pass

            if server_runner.process is None or client_runner.process is None:
                continue
            if server_runner.process.stdout is None or client_runner.process.stdout is None:
                continue

            server_output = server_runner.process.stdout.read(2000)
            if server_output:
                server_stdout += server_output

            client_output = client_runner.process.stdout.read(2000)
            if client_output:
                client_stdout += client_output

        if server_runner.process is not None and client_runner.process is not None:
            # Close stdouts
            if server_runner.process.stdout is not None and client_runner.process.stdout is not None:
                server_runner.process.stdout.close()
                client_runner.process.stdout.close()

            # Kill processes
            os.system("pkill -P " + str(server_runner.process.pid))
            os.system("pkill -P " + str(client_runner.process.pid))
            server_runner.process.kill()
            client_runner.process.kill()
            server_runner.process.wait()
            client_runner.process.wait()

            # Close files
            server_runner.stderr_file.close()
            client_runner.stderr_file.close()
            server_runner.f.close()
            client_runner.f.close()

        s.close()
        c.close()

        
        return (server_packets, client_packets, (server_stdout, client_stdout, file))

    @partial_credit(5)
    @number(1.1)
    @hide_errors()
    def test_client_syn(self, set_score):
        """Handshake: Client SYN"""
        if test_0_compilation.failed:
            self.fail()

        sp, cp, _ = self.make_test(self.test_client_syn.__name__, True)

        if sp >= 1:
            set_score(5)
        elif cp >= 1:
            print("Your Client SYN is in an unrecognized form.")
            set_score(2.5)
            self.fail()
        else:
            print("Your client failed to send over a Client SYN.")
            set_score(0)
            self.fail()

    @partial_credit(5)
    @number(1.2)
    @hide_errors()
    def test_server_synack(self, set_score):
        """Handshake: Server SYN-ACK"""
        if test_0_compilation.failed:
            self.fail()

        sp, cp, _ = self.make_test(self.test_server_synack.__name__, False)

        if cp >= 2:
            set_score(5)
        elif sp >= 1:
            print("Your Server SYN-ACK is in an unrecognized form.")
            set_score(2.5)
            self.fail()
        else:
            print("Your server failed to send over a Server SYN-ACK.")
            set_score(0)
            self.fail()

    @partial_credit(5)
    @number(1.3)
    @hide_errors()
    def test_client_ack(self, set_score):
        """Handshake: Client ACK"""
        if test_0_compilation.failed:
            self.fail()

        sp, cp, _ = self.make_test(self.test_client_ack.__name__, True)

        if cp >= 2 and sp >= 1:
            set_score(5)
        else:
            print("Your client failed to send over a Client ACK.")
            set_score(0)
            self.fail()
