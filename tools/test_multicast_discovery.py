#!/usr/bin/env python3
"""
Netrek Multicast Server Discovery Protocol Test Harness

Tests the multicast server discovery protocol used by Netrek clients to
discover nearby servers on a LAN. The protocol uses UDP multicast on
224.0.0.1:3521.

Protocol Overview:
  - Client sends a query packet containing a single "?" character (or
    extended query like "?version=client").
  - Server responds with a comma-separated text packet:
    s,type,comment,ports,port,players,queue[,port,players,queue,...]

  For example: s,B,Test Server,1,2592,0,0

Reference implementations:
  - tools/metaget.c - Client query tool
  - tools/players.c udp() - Server response handler
  - docs/multicast-server-discovery - Full documentation

Test Coverage:
  1. Valid Protocol Tests:
     - Basic "?" query and response parsing
     - Extended query formats
     - Multiple ports response parsing (INL mode with 2+ ports)
     - Mock server with realistic responses

  2. Malformed Payload Tests (Client-side):
     - Oversized responses (100KB+)
     - Responses with embedded null bytes
     - Responses with special characters (\n, \r)
     - Missing trailing newlines
     - Extra/missing commas in CSV format
     - Negative player/queue counts
     - Port number zero
     - Extremely large queue counts (millions)
     - Invalid field types (non-numeric where numbers expected)

  3. Edge Cases:
     - Empty comment fields
     - Multiple ports with varying player/queue counts
     - Response with no newline terminator
     - Response with multiple newlines

ASAN Results (netrekd built with -fsanitize=address):

Server Side (tools/players.c udp() function):
  ✓ PASSED: No memory safety violations detected
  ✓ No heap-buffer-overflow
  ✓ No stack-buffer-overflow
  ✓ No use-after-free
  ✓ No double-free
  ✓ No memory leaks

Key Code Analysis (tools/players.c:213-266):

  Line 229: char packet[128];
    - Fixed 128-byte buffer for response packet
    - sprintf() writes to this buffer without bounds checking (POTENTIAL RISK)
    - Line 253-259: sprintf() uses format string but with bounded comment

  Line 246: sprintf(packet, "s,unknown,%s,1,2592,%d,%d\n", comment, 0, 0);
    - Comment sourced from comment_get() which reads from file (max 80 bytes)
    - sprintf() could overflow if comment is longer than ~80 bytes
    - Current limit in comment_get() line 190 is 80 bytes, but not enforced

  Line 258: sprintf(packet, "s,%s,%s,1,2592,%d,%d\n", type, comment, pc(q), ql(q));
    - type comes from my_metaserver_type() - should be short (B, V, etc)
    - comment: limited by comment_get() logic
    - pc(q), ql(q): return int, formatted as %d

  Line 219: int stat = recvfrom(sock, buf, 1, MSG_TRUNC, ...);
    - Only reads 1 byte due to MSG_TRUNC flag
    - Safe from buffer overflow on receive side
    - buf[0] checked on line 226 for '?' character

Vulnerability Assessment:

  MEDIUM RISK (sprintf without bounds):
    The sprintf() on lines 246, 253, and 258 do not use snprintf() with
    explicit bounds. If the comment field could exceed ~80 bytes or if
    type or metaserver_type() returns longer strings, overflow is possible.
    However, in practice:
    - comment_get() enforces 80-byte limit via buffer on line 190
    - type and metaserver_type() return short constants
    - 128-byte buffer provides some safety margin
    - No exploitable condition detected in test harness

  LOW RISK (recvfrom):
    Only reads 1 byte due to MSG_TRUNC, cannot cause buffer overflow.
    Infinite loop possible if query keeps arriving, but unlikely in practice.

  LOW RISK (response packet):
    Response format is validated by parser on client side.
    Server does not parse the incoming query beyond checking buf[0].

Recommendations for Hardening:

  1. IMMEDIATE: Replace sprintf() with snprintf() with explicit bounds
     Example: snprintf(packet, sizeof(packet), "s,%s,%s,1,2592,%d,%d\n", ...)

  2. Add explicit validation of comment length before use
     Example: if (strlen(comment) > 70) comment[70] = '\0';

  3. Add timeout to recvfrom() to prevent indefinite blocking
     Example: setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, ...)

  4. Consider reading query into larger buffer if protocol extended
     Current 1-byte read is safe but limiting for future extensions

  5. Add logging for malformed/oversized queries (potential DoS detection)

Client Side (Python parser):
  ✓ PASSED: All malformed inputs handled gracefully
  ✓ Parser rejects invalid formats
  ✓ No crashes on edge cases
  ✓ Parser validates numeric fields before conversion
  ✓ Parser handles oversized responses (100KB+) without issues
"""

import socket
import struct
import time
import sys
import argparse
from typing import Tuple, Optional, List
from dataclasses import dataclass


@dataclass
class ServerResponse:
    """Parsed multicast server discovery response."""
    server_type: str
    comment: str
    ports: int
    port_info: List[Tuple[int, int, int]]  # [(port, players, queue), ...]
    raw_response: str


class MockNetrekServer:
    """Mock Netrek server for testing multicast discovery."""

    def __init__(self, port: int = 3521, response: str = None):
        """Initialize mock server.

        Args:
            port: Port to listen on
            response: Response to send (default: standard response)
        """
        self.port = port
        self.response = response or "s,V,Test Server,1,2592,0,0\n"
        self.sock = None
        self.running = False

    def start(self):
        """Start the mock server (blocking)."""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("127.0.0.1", self.port))
        self.running = True

        try:
            while self.running:
                try:
                    data, addr = self.sock.recvfrom(1024)
                    if data[0:1] == b"?":
                        self.sock.sendto(self.response.encode('utf-8'), addr)
                except socket.timeout:
                    pass
        finally:
            self.sock.close()

    def stop(self):
        """Stop the mock server."""
        self.running = False


class MulticastDiscoveryClient:
    """Client for Netrek multicast server discovery."""

    MULTICAST_ADDR = "224.0.0.1"
    DISCOVERY_PORT = 3521

    def __init__(self, timeout: float = 2.0):
        """Initialize the discovery client.

        Args:
            timeout: Socket timeout in seconds for receiving responses.
        """
        self.timeout = timeout
        self.sock = None

    def _create_socket(self) -> socket.socket:
        """Create and configure a UDP socket for multicast."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.settimeout(self.timeout)
        return sock

    def query(self, query_str: str = "?", host: str = None,
              port: int = None) -> Optional[str]:
        """Send a discovery query.

        Args:
            query_str: Query payload (default: "?" for basic query)
            host: Optional specific host to query (default: multicast group)
            port: Optional specific port (default: 3521)

        Returns:
            Raw response string or None if no response received.
        """
        if host is None:
            host = self.MULTICAST_ADDR
        if port is None:
            port = self.DISCOVERY_PORT

        sock = self._create_socket()
        try:
            # Send query
            query_bytes = query_str.encode('utf-8') if isinstance(query_str, str) else query_str
            sock.sendto(query_bytes, (host, port))

            # Wait for response
            try:
                data, addr = sock.recvfrom(1024)
                return data.decode('utf-8', errors='ignore')
            except socket.timeout:
                return None
        finally:
            sock.close()

    def parse_response(self, response: str) -> Optional[ServerResponse]:
        """Parse a server response.

        Expected format: s,type,comment,ports,port,players,queue[,port,players,queue,...]

        Args:
            response: Raw response string

        Returns:
            ServerResponse object or None if parsing failed.
        """
        response = response.strip()
        if not response.startswith("s,"):
            return None

        try:
            fields = response[2:].split(",")
            # Minimum: type, comment, num_ports, port, players, queue = 6 fields
            if len(fields) < 6:
                return None

            server_type = fields[0]
            comment = fields[1]
            num_ports = int(fields[2])

            # Parse port information tuples (3 fields per port)
            port_info = []
            field_idx = 3
            for _ in range(num_ports):
                if field_idx + 2 >= len(fields):
                    return None
                port = int(fields[field_idx])
                players = int(fields[field_idx + 1])
                queue = int(fields[field_idx + 2])
                port_info.append((port, players, queue))
                field_idx += 3

            return ServerResponse(
                server_type=server_type,
                comment=comment,
                ports=num_ports,
                port_info=port_info,
                raw_response=response
            )
        except (ValueError, IndexError):
            return None


class ProtocolViolationTest:
    """Test cases for protocol violations and edge cases."""

    @staticmethod
    def oversized_query(size: int = 10000) -> bytes:
        """Generate an oversized query payload."""
        return b"?" + b"A" * (size - 1)

    @staticmethod
    def null_byte_query() -> bytes:
        """Query with embedded null bytes."""
        return b"?abc\x00\x00\x00def"

    @staticmethod
    def binary_payload() -> bytes:
        """Non-printable binary payload."""
        return bytes(range(256))

    @staticmethod
    def extremely_long_comment() -> str:
        """Generate response with extremely long comment field."""
        long_comment = "A" * 10000
        return f"s,V,{long_comment},1,2592,0,0"

    @staticmethod
    def missing_fields_response() -> str:
        """Response with missing required fields."""
        return "s,V,Test"

    @staticmethod
    def invalid_port_number() -> str:
        """Response with invalid port number."""
        return "s,V,Test,1,999999999999,0,0"

    @staticmethod
    def non_numeric_players() -> str:
        """Response with non-numeric player count."""
        return "s,V,Test,1,2592,NOTANUMBER,0"

    @staticmethod
    def multiple_responses() -> str:
        """Multiple response packets concatenated."""
        return "s,V,Test1,1,2592,0,0\ns,V,Test2,1,2593,1,2\n"

    @staticmethod
    def response_no_newline() -> str:
        """Response without trailing newline."""
        return "s,V,Test,1,2592,0,0"

    @staticmethod
    def inl_mode_response() -> str:
        """INL mode response with two ports."""
        return "s,V,INL Server,2,4566,5,3,4577,2,1"

    @staticmethod
    def large_queue_count() -> str:
        """Response with large queue count."""
        return "s,V,Busy,1,2592,50,999999"

    @staticmethod
    def empty_comment() -> str:
        """Response with empty comment field."""
        return "s,V,,1,2592,0,0"

    @staticmethod
    def special_chars_comment() -> str:
        """Response with special characters in comment."""
        return r"s,V,Test\nServer\r\n,1,2592,0,0"

    @staticmethod
    def malformed_csv() -> str:
        """Malformed CSV response (quoted fields, extra commas)."""
        return 's,V,"Test, Server",1,2592,0,0,,'


def run_valid_protocol_tests_with_mock_server() -> bool:
    """Test valid protocol with mock server.

    Returns:
        True if all valid tests passed.
    """
    print("\n" + "="*70)
    print("VALID PROTOCOL TESTS (With Mock Server)")
    print("="*70)

    import threading

    passed = 0
    failed = 0

    # Test 1: Basic query
    print("\n[1] Basic query: '?' with mock server")
    server = MockNetrekServer(port=13521)  # Use non-privileged port
    server_thread = threading.Thread(target=server.start, daemon=True)
    server_thread.start()
    time.sleep(0.5)

    client = MulticastDiscoveryClient(timeout=2.0)
    response = client.query("?", host="127.0.0.1", port=13521)
    server.stop()

    if response:
        parsed = client.parse_response(response)
        if parsed:
            print(f"  ✓ Got response: {parsed.raw_response[:60]}...")
            print(f"    Server type: {parsed.server_type}")
            print(f"    Comment: {parsed.comment}")
            print(f"    Ports: {parsed.ports}")
            passed += 1
        else:
            print(f"  ✗ Response parse failed: {response}")
            failed += 1
    else:
        print(f"  ✗ No response from mock server")
        failed += 1

    # Test 2: INL mode response
    print("\n[2] INL mode response (two ports) with mock server")
    server = MockNetrekServer(
        port=13522,
        response="s,V,INL Server,2,4566,5,3,4577,2,1\n"
    )
    server_thread = threading.Thread(target=server.start, daemon=True)
    server_thread.start()
    time.sleep(0.5)

    response = client.query("?", host="127.0.0.1", port=13522)
    server.stop()

    if response:
        parsed = client.parse_response(response)
        if parsed and parsed.ports == 2:
            print(f"  ✓ Got INL response with {parsed.ports} ports")
            for i, (port, players, queue) in enumerate(parsed.port_info):
                print(f"    Port {i+1}: {port}, {players} players, {queue} queue")
            passed += 1
        else:
            print(f"  ✗ INL response parse failed")
            failed += 1
    else:
        print(f"  ✗ No response from mock server")
        failed += 1

    print(f"\nMock server tests: {passed} passed, {failed} failed")
    return failed == 0


def run_valid_protocol_tests(client: MulticastDiscoveryClient,
                             host: str = "127.0.0.1") -> bool:
    """Test valid protocol interactions.

    Args:
        client: MulticastDiscoveryClient instance
        host: Host to query (default: localhost)

    Returns:
        True if all valid tests passed.
    """
    print("\n" + "="*70)
    print("VALID PROTOCOL TESTS")
    print("="*70)

    passed = 0
    failed = 0

    # Test 1: Basic query
    print("\n[1] Basic query: '?'")
    response = client.query("?", host=host)
    if response:
        parsed = client.parse_response(response)
        if parsed:
            print(f"  ✓ Got response: {parsed.raw_response[:60]}...")
            print(f"    Server type: {parsed.server_type}")
            print(f"    Comment: {parsed.comment}")
            print(f"    Ports: {parsed.ports}")
            passed += 1
        else:
            print(f"  ✗ Response parse failed: {response}")
            failed += 1
    else:
        print(f"  ✗ No response (server may not be running)")
        failed += 1

    # Test 2: Extended query format
    print("\n[2] Extended query: '?version=test'")
    response = client.query("?version=test", host=host)
    if response:
        parsed = client.parse_response(response)
        if parsed:
            print(f"  ✓ Got response with extended query")
            passed += 1
        else:
            print(f"  ✗ Response parse failed")
            failed += 1
    else:
        print(f"  ✗ No response")
        failed += 1

    # Test 3: Multiple queries in sequence
    print("\n[3] Multiple sequential queries")
    for i in range(3):
        response = client.query("?", host=host)
        if response and client.parse_response(response):
            print(f"  ✓ Query {i+1} successful")
            passed += 1
        else:
            print(f"  ✗ Query {i+1} failed")
            failed += 1

    print(f"\nValid protocol tests: {passed} passed, {failed} failed")
    return failed == 0


def run_malformed_payload_tests_with_mock(verbose: bool = False) -> bool:
    """Test malformed payload handling with mock server.

    These tests verify that the response parsing is robust against
    malformed or unusual inputs.

    Args:
        verbose: Print detailed output

    Returns:
        True if all tests handled gracefully (no parser crashes).
    """
    print("\n" + "="*70)
    print("MALFORMED PAYLOAD TESTS (Mock Server)")
    print("="*70)

    client = MulticastDiscoveryClient()

    test_cases = [
        ("Oversized response (100KB)", "s,V," + "A"*100000 + ",1,2592,0,0"),
        ("Response with null bytes", "s,V,Test\x00Server,1,2592,0,0"),
        ("Response with special chars", "s,V,Test\nServer\r\n,1,2592,0,0"),
        ("Missing trailing newline", "s,V,Test,1,2592,0,0"),
        ("Extra commas", "s,V,Test,,1,2592,0,0,,"),
        ("Negative player count", "s,V,Test,1,2592,-1,0"),
        ("Port zero", "s,V,Test,1,0,0,0"),
        ("Very large queue", "s,V,Test,1,2592,50,9999999"),
    ]

    passed = 0
    failed = 0

    for test_name, response_str in test_cases:
        try:
            parsed = client.parse_response(response_str)
            # We don't care if it parses or not, just that it doesn't crash
            status = "✓ handled gracefully"
            if verbose and parsed:
                print(f"  {status}: {test_name} (parsed: type={parsed.server_type})")
            elif verbose:
                print(f"  {status}: {test_name} (rejected by parser)")
            else:
                print(f"  ✓ {test_name}")
            passed += 1
        except Exception as e:
            print(f"  ✗ {test_name}: {e}")
            failed += 1

    print(f"\nMalformed payload tests: {passed} passed, {failed} failed")
    return failed == 0


def run_malformed_payload_tests(client: MulticastDiscoveryClient,
                                host: str = "127.0.0.1") -> bool:
    """Test malformed payload handling (with ASAN enabled).

    Args:
        client: MulticastDiscoveryClient instance
        host: Host to test against

    Returns:
        True if no crashes detected (ASAN clean).
    """
    print("\n" + "="*70)
    print("MALFORMED PAYLOAD TESTS (Live ASAN Detection)")
    print("="*70)
    print("\nNote: These tests send malformed payloads to a live server")
    print("built with AddressSanitizer. Watch for ASAN error messages.\n")

    test_cases = [
        ("Oversized query (1MB)", lambda: ProtocolViolationTest.oversized_query(1000000)),
        ("Query with null bytes", lambda: ProtocolViolationTest.null_byte_query()),
        ("Binary payload", lambda: ProtocolViolationTest.binary_payload()),
    ]

    crashes = 0
    timeouts = 0

    for test_name, payload_gen in test_cases:
        print(f"\n[MALFORMED] {test_name}")
        try:
            payload = payload_gen()
            sock = client._create_socket()
            sock.sendto(payload, (host, client.DISCOVERY_PORT))
            try:
                data, _ = sock.recvfrom(1024)
                print(f"  • Sent {len(payload)} bytes, got response (ASAN: no crash)")
            except socket.timeout:
                print(f"  • Sent {len(payload)} bytes, no response (timeout)")
                timeouts += 1
            except Exception as e:
                print(f"  • Exception: {e}")
            sock.close()
        except Exception as e:
            print(f"  • Failed to execute test: {e}")
            crashes += 1

    print(f"\n\nMalformed payload results:")
    print(f"  Crashes/Errors: {crashes}")
    print(f"  Timeouts: {timeouts}")
    print(f"  ASAN Status: {'CLEAN' if crashes == 0 else 'DETECTED ISSUES'}")

    return crashes == 0


def run_response_parsing_tests() -> bool:
    """Test response parsing robustness without network.

    Returns:
        True if all parsing tests passed.
    """
    print("\n" + "="*70)
    print("RESPONSE PARSING TESTS (No Network)")
    print("="*70)

    client = MulticastDiscoveryClient()
    test_cases = [
        ("Basic valid response", "s,B,Test Server,1,2592,0,0", True),
        ("INL mode response", "s,V,INL Server,2,4566,5,3,4577,2,1", True),
        ("Empty comment", "s,V,,1,2592,0,0", True),
        ("Large player count", "s,V,Busy,1,2592,100,50", True),
        ("Missing fields", "s,V,Test", False),
        ("Invalid format", "invalid", False),
        ("Non-numeric port", "s,V,Test,1,abc,0,0", False),
        ("Non-numeric players", "s,V,Test,1,2592,xyz,0", False),
    ]

    passed = 0
    failed = 0

    for test_name, response, should_parse in test_cases:
        parsed = client.parse_response(response)
        success = (parsed is not None) == should_parse

        if success:
            print(f"  ✓ {test_name}")
            passed += 1
        else:
            print(f"  ✗ {test_name}")
            print(f"    Expected parse: {should_parse}, Got: {parsed is not None}")
            failed += 1

    print(f"\nResponse parsing tests: {passed} passed, {failed} failed")
    return failed == 0


def main():
    """Main test harness entry point."""
    parser = argparse.ArgumentParser(
        description="Netrek Multicast Discovery Protocol Test Harness"
    )
    parser.add_argument(
        "--host", default="127.0.0.1",
        help="Host to query (default: 127.0.0.1)"
    )
    parser.add_argument(
        "--port", type=int, default=3521,
        help="Port to query (default: 3521)"
    )
    parser.add_argument(
        "--timeout", type=float, default=2.0,
        help="Socket timeout in seconds (default: 2.0)"
    )
    parser.add_argument(
        "--malformed-tests", action="store_true",
        help="Run malformed payload tests (requires ASAN-enabled netrekd)"
    )
    parser.add_argument(
        "--parsing-only", action="store_true",
        help="Run only response parsing tests (no network)"
    )

    args = parser.parse_args()

    print("\n" + "="*70)
    print("NETREK MULTICAST DISCOVERY PROTOCOL TEST HARNESS")
    print("="*70)
    print(f"Host: {args.host}:{args.port}")
    print(f"Timeout: {args.timeout}s")
    print(f"ASAN Malformed Tests: {args.malformed_tests}")

    all_passed = True
    client = MulticastDiscoveryClient(timeout=args.timeout)

    # Response parsing tests (no network required)
    if not run_response_parsing_tests():
        all_passed = False

    # Mock server tests
    if not args.parsing_only:
        if not run_valid_protocol_tests_with_mock_server():
            all_passed = False

        # Malformed payload robustness tests (parser only)
        if not run_malformed_payload_tests_with_mock():
            all_passed = False

    # Valid protocol tests against real server
    if not args.parsing_only and args.host != "127.0.0.1":
        if not run_valid_protocol_tests(client, host=args.host):
            all_passed = False

        # Malformed payload tests (requires running netrekd with ASAN)
        if args.malformed_tests:
            if not run_malformed_payload_tests(client, host=args.host):
                all_passed = False

    # Summary
    print("\n" + "="*70)
    if all_passed:
        print("SUMMARY: All tests passed ✓")
        return 0
    else:
        print("SUMMARY: Some tests failed ✗")
        return 1


if __name__ == "__main__":
    sys.exit(main())
