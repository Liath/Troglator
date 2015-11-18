# Troglator: A VLC to WinAmp API Plugin
## Premise
Many apps can talk to Winamp/Foobar2k through their common interface. IRC and other chat clients mostly. This plugin lets VLC speak this common language.
My goal is to get my Logitech G15's LCD to show VLC info in an app I use that speaks WinAmp.

### SirReals G15 App
LCDSirReal has lived on my G15 since day one, probably about five years at time of writing. Here's a link to it: http://www.linkdata.se/software/lcdsirreal/
Note the Winamp support. It's really quite convenient. But I haven't had a use for winamp since before I even owned a G15. Like most sane human beings, I use VLC.

## WinAmpian Primer
The WinAmpian language is spoken over Windows SendMessage (Relevant). Here's the only useful documentation I've come across on the matter: http://forums.winamp.com/showthread.php?threadid=180297

## Disassembling for Fun and Profit
First, as we have no clue what commands are actually relevant to LCDSirReal, we're going to hunt down the conversation between it and WinAmp. Unfortunately, this means we need WinAmp installed and running for now.
OllyDbg is my tool of choice for this, just pop lcdsirreal open in it and it'll look something like this:
![OllyDbg opening LCDSirReal](http://puu.sh/ckd1i/ad06d2818d.png)

Now we need to hunt down all SendMessage calls (Right Click -> Search For -> All Intermodular Calls):
![Intermodular Calls showing a small number of SendMessage commands](http://puu.sh/ckdbA/41aa99f298.png)

Wow! There's only three. This tells us that we won't be searching long, so we drop breakpoints on them on hit play.
![Hole in One (Like it was a hard guess. :P)](http://puu.sh/ckdvC/56661964b0.png)
This screen reveals some nifty things right away. We can clearly see some value from WinAmp are loaded already and we see a couple SendMessage. Our reference material above translates the first into (data=0, id=105) which is a lookup for the current tracks position in ms. The SendMessage slightly further down is (data=1, id=105) which looks up the total length of the current track. The third and final SendMessage (data=1, id=126) looks up the bitrate. So we know we need to emulate these commands. 

But where is LCDSirReal getting the track name and, for that matter, the handle (hWnd) of WinAmp? No clue, so I turn to the first place I look when I'm lost. Strings! (Right Click in CPU screen -> Search for -> All referenced text strings) 
![Flying String Monster, so much output.](http://puu.sh/ckejA/40e9fc0ab8.png)
There's a couple leads here. Let's break them and restart the process.

Here's our handle:
![A wild Handle appears!](http://puu.sh/ckeqX/66ab53563b.png)

And on the next string breakpoint we see that the info has been found somewhere prior.
![Avast!](http://puu.sh/ckez3/b9218b155c.png)

I find that GetWindowTextW suspect. Let's take a looksie:
![Twas a safe bet, no?](http://puu.sh/ckeIT/4f946ab5d7.png)
And there it is. We now know we need to create a window of class "Winamp v1.x" with a title in the format of "(track number). (artist) - (song name) - Winamp" that will then receive our SendMessages.

#### The Code


#### Getting VLC to compile
Or at least enough of it that we can do in-tree cross-compilation. This applies to cross-compiling VLC for Windows in general since the wiki is kind of... unhelpful.
```
apt-get install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 mingw-w64-tools
git clone git://git.videolan.org/vlc.git vlc
cd vlc
mkdir -p contrib/win32
cd contrib/win32
../bootstrap --host=x86_64-w64-mingw32
```
####Set path options (I don't honestly know which of these were necesary but some combination of them made things work)
```
export CPLUS_INCLUDE_PATH=/usr/x86_64-w64-mingw32/include
export LDFLAGS=-L/usr/x86_64-w64-mingw32/lib
export DFLAGS=-L/usr/x86_64-w64-mingw32/include
export PKG_CONFIG_LIBDIR=/path/to/vlc-git/contrib/x86_64-w64-mingw32/lib/pkgconfig
export PKG_CONFIG_PATH=/path/to/vlc-git/contrib/x86_64-w64-mingw32

make prebuilt
rm ../x86_64-w64-mingw32/bin/moc ../x86_64-w64-mingw32/bin/uic ../x86_64-w64-mingw32/bin/rcc
ln -sf x86_64-w64-mingw32 ../i686-w64-mingw32
cd -
./bootstrap
mkdir win32 && cd win32

# Disable EVERYTHING we can
# This is helpful to cut out the cruft: ../configure --help | grep -oE "\-\-((en|dis)able|with(out|))\-[a-zA-Z0-9-]+" | sed 's/--en/--dis/' | sed 's/--with-/--without-/' | tr '\n' ' '
../configure --host=x86_64-w64-mingw32 --disable-live555 --disable-dc1394 --disable-dv1394 --disable-linsys --disable-dvdread --disable-dvdnav --disable-bluray --disable-opencv --disable-smbclient --disable-sftp --disable-v4l2 --disable-decklink --disable-gnomevfs --disable-vcdx --disable-vcd --disable-libcddb --disable-screen --disable-vnc --disable-freerdp --disable-realrtsp --disable-macosx-eyetv --disable-macosx-qtkit --disable-macosx-avfoundation --disable-asdcp --disable-dvbpsi --disable-gme --disable-sid --disable-ogg --disable-mux_ogg --disable-shout --disable-mkv --disable-mod --disable-mpc --disable-wma-fixed --disable-shine --disable-omxil --disable-omxil-vout --disable-rpi-omxil --disable-mmal-codec --disable-crystalhd --disable-mad --disable-merge-ffmpeg --disable-gst-decode --disable-avcodec --disable-libva --disable-dxva2 --disable-vda --disable-avformat --disable-swscale --disable-postproc --disable-faad --disable-vpx --disable-twolame --disable-fdkaac --disable-quicktime --disable-a52 --disable-dca --disable-flac --disable-libmpeg2 --disable-vorbis --disable-tremor --disable-speex --disable-opus --disable-theora --disable-schroedinger --disable-png --disable-jpeg --disable-x262 --disable-x265 --disable-x26410b --disable-x264 --disable-mfx --disable-fluidsynth --disable-zvbi --disable-telx --disable-libass --disable-kate --disable-tiger --disable-gles2 --disable-gles1 --disable-xcb --disable-xvideo --disable-glx --disable-vdpau --disable-sdl --disable-sdl-image --disable-freetype --disable-fribidi --disable-fontconfig --disable-macosx-quartztext --disable-svg --disable-svgdec --disable-android-surface --disable-directx --disable-directfb --disable-aa --disable-caca --disable-kva --disable-mmal-vout --disable-pulse --disable-alsa --disable-oss --disable-sndio --disable-wasapi --disable-audioqueue --disable-jack --disable-opensles --disable-samplerate --disable-kai --disable-chromaprint --disable-skins2 --disable-libtar --disable-macosx --disable-minimal-macosx --disable-macosx-dialog-provider --disable-ncurses --disable-lirc --disable-goom --disable-projectm --disable-vsxu --disable-atmo --disable-glspectrum --disable-bonjour --disable-udev --disable-mtp --disable-upnp --disable-libxml2 --disable-libgcrypt --disable-gnutls --disable-taglib --disable-update-check --disable-growl --disable-macosx-vlc-app --disable-lua --disable-vlc --disable-qt

./compile
#Despite all the disables, a bunch of modules will still get built that we don't need. I just removed the ones that were holding up compilation from their respective category's Makefile.am then compiled again
./compile
```
Once vlc is compiled we can add our file into the build without much fuss.
```
#Add our module to the notify Makefile with something like this:
echo '#Trog~n
if HAVE_WIN32
libtrog_plugin_la_SOURCES = notify/troglator.c
libtrog_plugin_la_CFLAGS = $(AM_CFLAGS) $(NOTIFY_CFLAGS)
libtrog_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(notifydir)'
notify_LTLIBRARIES += libtrog_plugin.la
endif' >> ../modules/notify/Makefile.am

#Copy our code to where we told the Makefile
cp /path/to/trog/troglator.c /path/to/vlc-git/modules/notify/
./compile
```
