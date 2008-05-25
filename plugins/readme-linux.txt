To compile QVM's in Linux, you need the Quake3 Sourcecode to compile 4 binaries from it.. the Q3 sourcode can be fetched from google or here: http://www.idsoftware.com/business/techdownloads/

You also need "dos2unix" or anotherwise known as "fromdos" to modify some files which have DOS encoding in them that need to be changed to Linux style. (sudo apt-get install dos2unix or something like that anyway)

In the "/quake3-1.32b/lcc/src/" directory, run "fromdos" on all the .md files (fromdos *.md) otherwise "rcc" will not compile.

Now to the "/quake3-1.32b/lcc/" directory, need to build 3 targets:

make rcc (once compiled will be in /tmp/ as "rcc", you need to rename it to "q3rcc" and move it somewhere Linux can see it like /usr/bin)

make cpp (once compiled will be in /tmp/ as "cpp", you need to rename it to "q3cpp" and move it somewhere Linux can see it like /usr/bin/)

make lcc (once compiled will be in /tmp/ as "lcc", dont rename it, but move it somewhere Linux can see it like /usr/bin/)

Now to the "/quake3-1.32b/q3asm/" directory, the last target:

make (will be compiled in the same directory, move it somewhere Linux can see it like /usr/bin/)

With these 4 binaries built from the Quake3 source, should be able to now go to each Plugin's directory and type "make qvm"... if it works out you should have a "pluginname.qvm".

NOTE: If you are using the Icculus q3asm tool, you must specify "-vq3" before building.
NOTE2: FTE now has its own q3asm tool called "q3asm2" which you can find in the SVN repository.

~ Moodles (any questions about this guide ask me)

