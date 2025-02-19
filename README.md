# Cats

## Design
- Hard code as much of the 3-way handshake as possible...ended up being a bit more complicated depending on if ACKs had data in them or not but whatever
- The main listen_loop is the same across both server and client b/c they end up being the same after the hardcoded stuff

## Problems and Solutions:
- Flags are not big endian, took some wiresharking (without the Lua packet dissector so that shit was ass) and gdb-ing to figure that out
- C is very annoying when trying to implement a sending and receiving buffer, I'm using C++ vectors and making sure that they are always sorted so that when an ACK comes in, I can just remove everything from the ACKed packets backwards

## Notes:
- Would help to mention that straight ACK's (no data in them) all have SEQ 0 and don't increment any seq counter :/