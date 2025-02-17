# Cats

## Problems and Solutions:
- Flags are not big endian, took some wiresharking (without the Lua packet dissector so that shit was ass), gdb-ing to figure that out
- C is very annoying when trying to implement a sending and receiving buffer, I'm using C++ vectors and making sure that they are always sorted so that when an ACK comes in, I can just remove everything from the ACKed packets backwards