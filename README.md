ad1000
======

Telenet AD1000 Frontpanel Pseudo Driver

Installation
============

Download and install BCM2835 driver first.
Install from here : http://www.airspayce.com/mikem/bcm2835/index.html

After that:

       git clone git://github.com/glorang/ad1000
       make
       make install
       /usr/local/bin/ad1000


Usage
=====

ad1000 is the "master" daemon that will create following devices files:
- /dev/ad1000/led{1..3}
- /dev/ad1000/disp
- /dev/ad1000/disp_brightness

You can echo 1 or 0 in any led device file to turn it on or off. 

led1 is a dual color led, so in this device file you can echo :
- 1 for green
- 2 for red
- 3 for both green and red (orange-like)

You can echo all letters, numbers, hyphen and space into the disp device file.
The dot can be enabled by adding a dot in the input string. e.g. 'echo 12.34 > /dev/ad1000/disp'.

The ad1000 daemon will spawn off 2 childeren by default:

- lirc_led : this will flash LED2 each time an IR signal is received
- ad_display : this will connect to XBMC's TCP socket and update the display when:

	- you press PLAY/PAUSE/STOP 

	- music is playing it will marquee "artist - title" after which it will update to <current track>.<total tracks>

	- a video is playing it will marquee the filename


License
=======

                DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE 
                           Version 2, December 2004 
  
      Copyright (c) 2014 Geert Lorang
      
      Everyone is permitted to copy and distribute verbatim or modified 
      copies of this license document, and changing it is allowed as long 
      as the name is changed. 
    
                  DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE 
       TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION 
     
       0. You just DO WHAT THE FUCK YOU WANT TO.

The display part uses cJSON <http://sourceforge.net/projects/cjson/> which is licensed as follows:

       Copyright (c) 2009 Dave Gamble
       
       Permission is hereby granted, free of charge, to any person obtaining a copy
       of this software and associated documentation files (the "Software"), to deal
       in the Software without restriction, including without limitation the rights
       to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
       copies of the Software, and to permit persons to whom the Software is
       furnished to do so, subject to the following conditions:
       
       The above copyright notice and this permission notice shall be included in
       all copies or substantial portions of the Software.
       
       THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
       IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
       FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
       AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
       LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
       OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
       THE SOFTWARE.

