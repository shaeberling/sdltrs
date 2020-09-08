To build with autotools:
------------------------

From the main directory, execute:
```sh
./autogen.sh
```
which will generate the `configure` script. Installation of `aclocal`,
`autoconf` and `automake` is needed for this.

To configure the build system, execute:
```sh
./configure
```
To enable faster but not accurate Z80 block moves, execute:
```sh
./configure --enable-fastmove
```
To build with the integrated Z80 debugger zbx, execute:
```
./configure --enable-zbx
```
To enable `readline` support for the zbx debugger, execute:
```sh
./configure --enable-zbx --enable-readline
```

Start build of the program in the main directory by executing:
```sh
make
```

---

To build with SDL2:
-------------------

**SDL2TRS** needs the development files of `SDL2` and `GNU readline`
for the debugger. On *Debian* or *Ubuntu* these can be installed with:
```sh
sudo apt install libsdl2-dev libreadline-dev
```
From the `src` directory, execute:
```sh
make sdl2
```

---

To build on FreeBSD/NetBSD/OpenBSD:
-----------------------------------

From the `src` directory, execute:
```sh
make bsd
```

---

To build on macOS:
------------------

Download and install [Homebrew] for macOS first.
```sh
brew install autoconf automake libtool llvm readline sdl2
```
should download and install the required packages to build **SDL2TRS**.
In the main directory of the source, execute the following commands:
```sh
./autogen.sh
./configure --enable-readline
make
```

This will build the executable binary file called `sdl2trs`.

---

To build on Win32:
------------------

**SDL2TRS** is designed to be build with [MinGW]. The [SDL2] development
library is also required.

The runtime library file `SDL2.DLL` should be copied to the directory of
the **SDL2TRS** binary file, the header files of the library to `\MinGW\
include\SDL2` and libraries to the `\MinGW\lib\` directory, or edit the
macros `LIBS` and `INCS` in `Makefile` to point to the location of the
SDL2 installation.

From the `src` directory, execute:
```sh
mingw32-make wsdl2
```

[Homebrew]: https://brew.sh
[MinGW]: http://www.mingw.org
[SDL2]: https://www.libsdl.org
