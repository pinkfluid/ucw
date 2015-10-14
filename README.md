# ucw
(U)niversal (C)cache (W)rapper

This is an earlier release of UCW, an attempt to create a single executable utility for automagically
adding CCACHE support to any compilation process without the need of modifying the Makefiles or
build scripts.

It works by pre-loading(LD_PRELOAD) a shared library that hooks on exec*() system calls. If the execution
of a C compiler is detected, it is redirect to CCACHE.

This version is a proof of concept not to be considered stable in any way. Currently only GCC on Linux is supported.
