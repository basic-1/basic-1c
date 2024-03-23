# distr directory  
  
## Brief  
  
The directory contains BASIC1 compiler software packages with binary executable files, samples and documents.  
  
## Software packages  
  
`b1c_lnx_amd64_gcc_1.0.0-XX.deb` - deb package for Linux, amd64 (Intel x64) architecture.  
`b1c_lnx_armhf_gcc_1.0.0-XX.deb` - deb package for Linux, armhf (armv7l) architecture (suitable for Raspbian, Armbian and maybe other Linux-based operating systems for ARM computers).  
`b1c_lnx_i386_gcc_1.0.0-XX.deb` - deb package for Linux, i386 (Intel x86) architecture.  
`b1c_lnx_amd64_gcc_1.0.0-XX.tar.gz` - tar.gz archive for Linux, amd64 (Intel x64) architecture.  
`b1c_lnx_armhf_gcc_1.0.0-XX.tar.gz` - tar.gz archive for Linux, armhf (armv7l) architecture (suitable for Raspbian, Armbian and maybe other Linux-based operating systems for ARM computers).  
`b1c_lnx_i386_gcc_1.0.0-XX.tar.gz` - tar.gz archive for Linux, i386 (Intel x86) architecture.  
`b1c_win_x64_mingw_1.0.0-XX.zip` - zip archive with executables, samples and documentation for Windows x64.  
`b1c_win_x86_mingw_1.0.0-XX.zip` - zip archive with executables, samples and documentation for Windows x86.  
  
`install.sh` - Linux script to install BASIC1 compiler from tar.gz archive.  
`uninstall.sh` - Linux shell script to uninstall BASIC1 compiler installed from tar.gz archive.  
  
The packages do not contain source code so download it from [https://github.com/basic-1/basic-1c](https://github.com/basic-1/basic-1c) if needed. `XX` in package file name stands for build number (git revision), e.g.: `b1c_lnx_amd64_gcc_1.0.0-12.deb`.  
  
## deb packages (for Linux)  
  
Command sample to install BASIC1 compiler from deb package with dpkg (on Debian and Ubuntu):  
`sudo dpkg -i b1c_lnx_amd64_gcc_1.0.0-12.deb`  
  
To uninstall previously installed package:  
`sudo dpkg -r b1c`  
  
## tar.gz packages (for Linux)  
  
To install BASIC1 compiler from tar.gz archive just copy its content to `/usr/local` directory:  
`tar -xzvf b1c_lnx_armhf_gcc_1.0.0-12.tar.gz -C /tmp`  
`sudo cp -r /tmp/b1c/* /usr/local`  
`rm -r /tmp/b1c`  

`install.sh` script uses the same commands so the simplest way to install compiler from tar.gz archive is running the script with the archive name as argument:  
`sudo ./install.sh b1c_lnx_armhf_gcc_1.0.0-12.tar.gz`  
  
To uninstall the complier installed from tar.gz archive use `uninstall.sh` script:  
`sudo ./uninstall.sh`  
  
## zip packages (for Windows)  
  
To install BASIC1 compiler on Windows extract content of corresponding zip package to a directory where you want it to be installed (e.g.: "C:\\basic-1c"). Add full path of bin directory to the PATH environment variable (for "C:\\basic-1c" installation directory the bin directory full path is "C:\\basic-1c\\bin").  
  