# vax

`vax` is a custom module/code injection ecosystem for Nintendo Switch games.

It's essentially a C++ port/improved version of [SaltyNX]().

## Code injection procedure

### vax (sysmodule)

This is the basic component of this project:

- On the one hand, it periodically scans for new-started processes.
  
  If the new process is an application and the SD card contains modules to load for that specific game, it hooks the game and starts with the injection process (explained below)

- On the other hand, it hosts three separate ports:
  
  - `vax:mod`: this port interface is used by `vboot`, `vloader` and any other custom module. It serves two main purposes:
    
    - Acting as a privileged service manager: the `GetServiceHandle(...)` command it serves is similar to SM's one as it actually does that internally, but through this sysmodule's full permissions.

      - For services like FS, which have their own permission system which works by calling a certain command after accessing it, vax calls it by itself, returning to the module a FS service with full permissions.
    
    - Allowing various funcionality specific to modules: currently only consists on the `MemoryCopy(...)` command, which works similar to a usual `memcpy` but allowing addresses the module can't directly interact with.

  - `vax:boot` and `vax:ldr`, port interfaces only meant to be accessed from `vboot` and `vloader` respectively.

  - As a note, SaltyNX would previously serve all these purposes under the `SaltySD` port and through a more primitive and unsafe IPC code. It'd also only provide SD card filesystem handles, while now this whole concept was redesigned to have whole full privileged services.

#### Resource limits and ports

Note that games tend to have (or just always have?) a maximum amount of ports which can be opened of `1`, which is meant to open SM and regularly access other services.

Thus, for all the ports which are hosted and accessed through this project and custom modules, the procedure is always to open-port/call-desired-command/close-port, so that after calling the desired command the single port session available is not used by anything, all this before the game starts so the ports can be accessed one-by-one.

Another consequence of this is that custom modules (through replaced funcions being called later after the game starts) can no longer access `vax:mod`, since games seem to (always do?) leave their SM session open, not allowing any more ports to be accessed. Thus, anything they might want to obtain from the port (like SD card access) must be done on the module entrypoint.

### vboot

When a process injection starts, `vax` finds random code regions in the game code and overwrites it with `vboot`'s segments, always after having made a backup of the original game code there. Then `vboot` is started by adjusting the process's main thread's PC.

Since `vboot` was badly placed in the game's memory (we randomly pasted it over game memory), its main purpose is to setup a basic heap through `SetHeapSize` (note that the game didn't even get to start so nothing was set up) where the actual injection entrypoint code, `vloader`, will be placed and executed.

After setting up this basic heap (which `vloader` will later extend to fit all modules to be loaded), `vboot` loads `vloader` there through `vax:boot` interface commands, and then executes it.

### vloader

This program is responsible for setting everything up and launching all custom modules.

First of all, the moment it starts it notifies `vax` that `vboot` finished through `vax:ldr` commands, so that the game code backup which was overwritten by this program can be restored. It also registers all the existing game modules and sets up newlib/libnx to properly work.

As it's explained below, this program uses an internal heap to be able to use standard libc(++) code.

After setting up the mentioned initial stuff, it proceeds to manually patch the target game's `SetHeapSize` and `GetInfo` SVCs to force the extra heap size we need for our modules when the game later attempts to initialize the heap, effectively extending it while keeping the module heap area untouched.

Then modules are loaded from `sdmc:/atmosphere/contents/<game-app-id>/vax/<any-elf-files>`, and they get launched one-by-one.

After finishing with this, it finally jumps to the game's startup code, normally starting the game with all the modules having done their job. Note that all modules and this program never get unloaded, since their code might be used later in-game (for instance, when replaced functions try to call module-code).

## Custom modules

Custom modules must be compiled using `libvmod` libs, which are the base libs for any kind of module. Even `vboot` (partially) and `vloader` have to make use of them, since they are the ones which set up everything correctly.

Custom modules have a certain set of limitations:

- Services must not be accessed the usual way: they must be obtained through `vax:mod` (more specifically by `vmod::sf::GetService(...)` or dedicated lib code, like `vmod::fs` for FS code)
- Thread/anything involving TLS must not be touched while in-game, but on the module's main routine (which, as explained above, gets called before the game has even started) it might not suppose any problems. However, better to fully avoid using it.
  - For instance, this implies that thread creation MUST be done using the game's thread code.
  - This is all due to libnx's TLs and Nintendo's TLS implementations being completely incompatible.

> TODO: continue with this