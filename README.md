# Cats

## Design
- Hard code as much of the 3-way handshake as possible...ended up being a bit more complicated depending on if ACKs had data in them or not but whatever
- The main listen_loop is the same across both server and client b/c they end up being the same after the hardcoded stuff
- Steps for development were: 3 step handshake -> unidirectionality looping win=1 -> bidirectionality looping win=1 -> retransmission timer win=1, dup acks win=1, increase win size to arbitrarily fixed, handle win size to whatever bullshit the reference binaries did (we really had to increment these shits by 500 bytes, not a perfect 1012 huh?)
- At the end, I gave up on trying to increase window sizes by non-1012 increments and just using the floor function

## Problems and Solutions:
- Flags are not big endian, took some wiresharking (without the Lua packet dissector so that shit was ass) and gdb-ing to figure that out
- C is very annoying when trying to implement a sending and receiving buffer (someone said to implement a linked list and I'm like you can't be serious, that's literally a whole (CS32?) project in itself), so I'm using C++ vectors and using auxillary functions so that when inserting and deleting, it is always kept in order; this way we don't need to "linear-scan" but rather use built in functions (lower_bound) for binary search and we can straight delete the left and right parts of it
- There's really no problems and solutions to this monstrosity of a project, just endless bashing your head, logging everything, and manually tracing out SEQ and ACK numbers until you tweak all the counters to mash up. Sometimes when I wanted to cry and give up, I would try to refactor the code which honestly helped a lot to clean down the main loop and make it readable.

## Notes:
- Would help to mention that straight ACK's (no data in them) all have SEQ 0 and don't increment any seq counter :/, it was only mentioned during the 3-way handshake

[![](/frickutianyuanyu.png)]