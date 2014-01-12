TeensyControls Plugin for X-Plane Flight Simulator
==================================================

This plugin for the X-Plane simulator allows sharing of flight simulator variables with Arduino sketches running on Teensy.  A video demo is available here.

http://www.youtube.com/watch?v=gVZtq7NBJOo

The primary discussion regarding this plugin's development is here:

http://forums.x-plane.org/index.php?s=2bd25a88f1732936c50e18439f6f645f&showtopic=55952


No Technical Support
--------------------

Since the development of Teensy 3.0 in late 2012, PJRC simply as not had the resources to continue developing this plugin.  This source code is being released in the hope it can benefit software developers and flight simulatation enthusiasts, who might be able to update is needed for newer versions of X-Plane.

There is *no technical support* available for this software.

THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


Building The Code
-----------------

A makefile is provided to build the code.  Simply type "make" on the command line to build the code.  There is no support for other build environments (eg, GUI-based IDEs).

The makefile requires the X-Plane SDK to be copied to the "SDK" folder.  The X-Plane SDK is not distrubuted with this code.  You must obtain it separately and copy its contents to the SDK folder.

You *need to edit the Makefile* to select your operating system.  Building on Macintosh requires the X-Code command line utilities, available from Apple's developer program (historically, available without paying the developer subscription).  Building on Linux may require installing the "libudev-dev" package.

If you get errors while compiling, the X-Plane forum is the place to ask for help.


Microsoft Windows Build is NOT Supported
----------------------------------------

PJRC built the plugin for Windows using the MinGW cross compiler running on Ubuntu Linux.

To install the necessary software on Ubuntu Linux, use one of these commands:

> sudo apt-get install mingw32 mingw32-binutils mingw32-runtime

> sudo apt-get install gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64

Then simply edit the makefile and run make, as with the other systems.  A script is provided to automate copying the compiled code to a Windows machine named "xp" on your local network.  Of course, that Windows machine must be running a FTP server.  If a password is required, you can automate it with ".netrc" in your home directory.

This plugin has *never* been compiled on a native Windows system.  Paul does not use Windows and will not be pleased if asked any questions regarding how to build this code on Windows.  There is no technical support of any kind for this code, but especially when it comes to native Windows compilation, please do not ask.  Building on Linux is the best way.


