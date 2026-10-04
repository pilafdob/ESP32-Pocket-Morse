#!/usr/bin/env python3
"""Pair two freshly flashed boards over USB, without saving or printing their key."""
import argparse
import secrets
import time
import serial

def query(port):
    port.reset_input_buffer()
    port.write(b"INFO\n")
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        line = port.readline().decode("ascii", errors="replace").strip()
        if line.startswith("MORSE_INFO "):
            _, role, mac, configured = line.split()
            return role, mac, configured == "1"
    raise RuntimeError("No firmware INFO response. Close serial monitors and flash the correct firmware.")

def provision(port, peer, key):
    port.write(b"PAIR " + peer.replace(":", "").encode() + b" " + key.hex().encode() + b"\n")
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        line = port.readline().decode("ascii", errors="replace").strip()
        if line == "PAIR_OK":
            return
        if line == "PAIR_REJECTED":
            raise RuntimeError("Pairing rejected; existing pair data is never overwritten.")
        if line == "PAIR_STORED_NOT_READY":
            raise RuntimeError("Key was stored but radio/inbox initialization failed. Do not re-run pairing or erase data; inspect serial logs and repair this board.")
    raise RuntimeError("No pairing confirmation. A partially paired set needs fresh pairing data on both devices.")

def initialize_inbox(port):
    port.write(b"INIT_INBOX\n")
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        line = port.readline().decode("ascii", errors="replace").strip()
        if line == "INBOX_OK":
            return
        if line == "INBOX_REJECTED":
            raise RuntimeError("Inbox initialization rejected. Existing/corrupt flash was not erased; inspect the board before continuing.")
    raise RuntimeError("No inbox initialization response")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--a", required=True, help="USB serial port of DEVICE_A")
    parser.add_argument("--b", required=True, help="USB serial port of DEVICE_B")
    args = parser.parse_args()
    if args.a == args.b:
        parser.error("Use two different USB ports")
    # No automatic erase, reflash, broadcast pairing, default key, or key file.
    with serial.Serial(args.a, 115200, timeout=0.3) as a, serial.Serial(args.b, 115200, timeout=0.3) as b:
        time.sleep(2)
        role_a, mac_a, configured_a = query(a)
        role_b, mac_b, configured_b = query(b)
        if role_a != "A" or role_b != "B" or mac_a == mac_b:
            raise RuntimeError("Expected distinct devices flashed as A and B; verify ports/build targets.")
        if configured_a or configured_b:
            raise RuntimeError("At least one device is already paired. Existing keys/counters were preserved.")
        initialize_inbox(a)
        initialize_inbox(b)
        key = secrets.token_bytes(32)
        provision(a, mac_b, key)
        provision(b, mac_a, key)
        del key
        print(f"Paired A ({mac_a}) and B ({mac_b}). Key was not printed or saved to disk.")
        print("Both devices can now communicate. USB host temporarily held the key in memory.")

if __name__ == "__main__":
    main()
