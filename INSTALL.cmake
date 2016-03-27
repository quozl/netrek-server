Instructions for installing the Vanilla Netrek Server using CMake

Summary
-------

        1)  install the dependent packages, which on Debian or Ubuntu are:

		cmake
		build-essential
		libgdbm-dev
		libncurses5-dev
		libglib1.2-dev
		libgmp3-dev

	2)  create a build directory

		mkdir build
		cd build

	3)  create the build tree

		cmake ..

	4)  compile

		make

	.)  type "make install",  *FAILS*
	.)  "cd" to the path you gave, or /usr/local/games/netrek,
	.)  "bin/netrekd" to start the server,
	.)  use a client to connect to the server.
