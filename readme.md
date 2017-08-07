emboy
=====

This is a WIP gameboy emulator, meant to be absurdly tiny, disregarding readability
and wide compatibility in favor of just being *crazy* small, source-wise. It might evolve into an exercise in code obfuscation, if it wouldn't be considered that already.

This requires a compiler supporting C11 features, particularly anonymous structs and
unions. Recent versions of GCC work fine.

Just type 'make' to build it.

Run it like this:
    ./emboy <romfile.gb>

At the moment, it does not have graphics, sound, or input, and only emulates about
36% of the gbz80 instructions. But it's a start.