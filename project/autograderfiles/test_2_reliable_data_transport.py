import unittest
import multiprocessing
import os

import test_0_compilation

from random import randbytes, choice
from string import ascii_letters
from gradescope_utils.autograder_utils.decorators import number, hide_errors, partial_credit
from utils import proxy, byte_diff, ProcessRunner


start_port = 8080


class TestRDT(unittest.TestCase):

    def make_test(self, size1: int, size2: int, timeout: int, use_ascii: bool, ref_client: bool, ref_server: bool, drop_rate: float, reorder_rate: float, corrupt_rate: float, name: str) -> float:
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

        # Start proxy
        p_thread = multiprocessing.Process(target=proxy, args=(
            client_port, server_port, drop_rate, reorder_rate, corrupt_rate))
        p_thread.start()

        # Generate random file to send
        if use_ascii:
            file1 = b''.join([choice(ascii_letters).encode()
                              for _ in range(size1)])
            file2 = b''.join([choice(ascii_letters).encode()
                              for _ in range(size2)])
        else:
            file1 = randbytes(size1)
            file2 = randbytes(size2)

        # Create server and client
        if ref_server:
            server_runner = ProcessRunner(
                f'/autograder/source/src/server {server_port}', file1, name + "_refserver.out")
        else:
            server_runner = ProcessRunner(
                f'runuser -u student -- {makefile_dir}/server {server_port}', file1, name + "_yourserver.out")

        if ref_client:
            client_runner = ProcessRunner(
                f'/autograder/source/src/client localhost {client_port}', file2, name + "_refclient.out")
        else:
            client_runner = ProcessRunner(
                f'runuser -u student -- {makefile_dir}/client localhost {client_port}', file2, name + "_yourclient.out")

        # Run both processes and stop when both have outputted the right amount
        # of bytes (or on timeout)
        ProcessRunner.run_two_until_size_or_timeout(
            server_runner, client_runner, size1, size2, timeout)
        p_thread.terminate()

        # Compare both the server and client output to original file
        server_byte_diff = byte_diff(file2, server_runner.stdout) 
        if file2 != server_runner.stdout:
            if not ref_server and not ref_client:
                print("Your server didn't produce the expected result.")
                print(
                    f"We inputted {size2} bytes in your client and we received {len(server_runner.stdout)} bytes ", end='')
            elif ref_server:
                print("Your client didn't send data back to our server correctly.")
                print(
                    f"We inputted {size2} bytes in your client and we received {len(server_runner.stdout)} bytes ", end='')
            else:
                print("Your server didn't receive data from our client correctly.")
                print(
                    f"We sent {size2} bytes and your server received {len(server_runner.stdout)} bytes ", end='')
            print(
                f'with a percent difference of {server_byte_diff}%')

        client_byte_diff = byte_diff(file1, client_runner.stdout) 
        if file1 != client_runner.stdout:
            if not ref_server and not ref_client:
                print("Your client didn't produce the expected result.")
                print(
                    f"We inputted {size1} bytes in your server and we received {len(client_runner.stdout)} bytes ", end='')
            elif ref_client:
                print("Your server didn't send data back to our client correctly.")
                print(
                    f"We inputted {size1} bytes in your server and we received {len(client_runner.stdout)} bytes ", end='')
            else:
                print("Your client didn't receive data from our server correctly.")
                print(
                    f"We sent {size1} bytes and your client received {len(client_runner.stdout)} bytes ", end='')
            print(
                f'with a percent difference of {client_byte_diff}%')

        return 1 - ((client_byte_diff + server_byte_diff) / (100 * 2))



    @partial_credit(20)
    @number(2.1)
    @hide_errors()
    def test_self(self, set_score):
        """Reliable Data Transport (Your Client <-> Your Server): Small file (10 KB)"""
        if test_0_compilation.failed:
            self.fail()

        file_size = 10000
        timeout = 3
        drop_rate = 0
        reorder_rate = 0
        corrupt_rate = 0
        points = 20

        d = self.make_test(file_size, file_size, timeout, False, False, False,
                       drop_rate, reorder_rate, corrupt_rate, self.test_self.__name__)
        set_score(points * d)

        if d != 1: self.fail()

    @partial_credit(20)
    @number(2.2)
    @hide_errors()
    def test_self_drop(self, set_score):
        """Reliable Data Transport (Your Client <-> Your Server): Medium file (100 KB), Drop (5%)"""
        if test_0_compilation.failed:
            self.fail()

        file_size = 100000
        timeout = 30
        drop_rate = 0.05
        reorder_rate = 1
        corrupt_rate = 0
        points = 20

        d = self.make_test(file_size, file_size, timeout, False, False, False,
                       drop_rate, reorder_rate, corrupt_rate, self.test_self_drop.__name__)
        set_score(points * d)
        
        if d != 1: self.fail()

    @partial_credit(5)
    @number(2.3)
    @hide_errors()
    def test_client(self, set_score):
        """Reliable Data Transport (Your Client <-> Reference Server): Small file (10 KB)"""
        if test_0_compilation.failed:
            self.fail()

        file_size = 10000
        timeout = 3
        drop_rate = 0
        reorder_rate = 0
        corrupt_rate = 0
        points = 5

        d = self.make_test(file_size, file_size, timeout, False, False, True,
                       drop_rate, reorder_rate, corrupt_rate, self.test_client.__name__)
        set_score(points * d)

        if d != 1: self.fail()

    @partial_credit(5)
    @number(2.4)
    @hide_errors()
    def test_server(self, set_score):
        """Reliable Data Transport (Reference Client <-> Your Server): Small file (10 KB)"""
        if test_0_compilation.failed:
            self.fail()

        file_size = 10000
        timeout = 3
        drop_rate = 0
        reorder_rate = 0
        corrupt_rate = 0
        points = 5

        d = self.make_test(file_size, file_size, timeout, False, True, False,
                       drop_rate, reorder_rate, corrupt_rate, self.test_server.__name__)
        set_score(points * d)

        if d != 1: self.fail()

    @partial_credit(5)
    @number(2.5)
    @hide_errors()
    def test_client_drop(self, set_score):
        """Reliable Data Transport (Your Client <-> Reference Server): Medium file (100 KB), Drop/Corrupt (5%)"""
        if test_0_compilation.failed:
            self.fail()

        file_size = 100000
        timeout = 30
        drop_rate = 0.05
        reorder_rate = 1
        corrupt_rate = 0.05
        points = 5

        d = self.make_test(file_size, file_size, timeout, False, False, True,
                       drop_rate, reorder_rate, corrupt_rate, self.test_client_drop.__name__)
        set_score(points * d)

        if d != 1: self.fail()


    @partial_credit(5)
    @number(2.6)
    @hide_errors()
    def test_server_drop(self, set_score):
        """Reliable Data Transport (Reference Client <-> Your Server): Medium file (100 KB), Drop/Corrupt (5%)"""
        if test_0_compilation.failed:
            self.fail()

        file_size = 100000
        timeout = 30
        drop_rate = 0.05
        reorder_rate = 1
        corrupt_rate = 0.05
        points = 5

        d = self.make_test(file_size, file_size, timeout, False, True, False,
                       drop_rate, reorder_rate, corrupt_rate, self.test_server_drop.__name__)
        set_score(points * d)

        if d != 1: self.fail()

    @partial_credit(5)
    @number(2.7)
    @hide_errors()
    def test_client_drop_large(self, set_score):
        """Reliable Data Transport (Your Client <-> Reference Server): Large file (500 KB), Drop (1%)"""
        if test_0_compilation.failed:
            self.fail()

        file_size = 500000
        timeout = 60
        drop_rate = 0.01
        reorder_rate = 1
        corrupt_rate = 0
        points = 5

        d = self.make_test(file_size, file_size, timeout, False, False, True,
                       drop_rate, reorder_rate, corrupt_rate, self.test_client_drop_large.__name__)
        set_score(points * d)

        if d != 1: self.fail()

    @partial_credit(5)
    @number(2.8)
    @hide_errors()
    def test_server_drop_large(self, set_score):
        """Reliable Data Transport (Reference Client <-> Your Server): Large file (500 KB), Drop (1%)"""
        if test_0_compilation.failed:
            self.fail()

        file_size = 500000
        timeout = 60
        drop_rate = 0.01
        reorder_rate = 1
        corrupt_rate = 0
        points = 5

        d = self.make_test(file_size, file_size, timeout, False, True, False,
                       drop_rate, reorder_rate, corrupt_rate, self.test_server_drop_large.__name__)
        set_score(points * d)

        if d != 1: self.fail()

    @partial_credit(5)
    @number(2.9)
    @hide_errors()
    def test_ginormous(self, set_score):
        """Reliable Data Transport (Your Client <-> Reference Server): Ginormous file (2 MB)"""
        if test_0_compilation.failed:
            self.fail()

        file_size = 2000000
        timeout = 60
        drop_rate = 0
        reorder_rate = 0
        corrupt_rate = 0
        points = 5

        d = self.make_test(file_size, file_size, timeout, False, False, True,
                       drop_rate, reorder_rate, corrupt_rate, self.test_ginormous.__name__)
        set_score(points * d)

        if d != 1: self.fail()

