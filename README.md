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

But where is LCDSirReal getting the track name and, for that matter the handle (hWnd) of WinAmp? No clue, so I turn to the first place I look when I'm lost. Strings! (Right Click in CPU screen -> Search for -> All referenced text strings) 
![Flying String Monster, so much output.](http://puu.sh/ckejA/40e9fc0ab8.png)
There's a couple leads here. Let's break them and restart the process.

Here's our handle:
![A wild Handle appears!](http://puu.sh/ckeqX/66ab53563b.png)

And on the next string breakpoint we see that the info has been found somewhere prior.
![Avast!](http://puu.sh/ckez3/b9218b155c.png)

I find that GetWindowTextW suspect. Let's take a looksie:
![Twas a safe bet, no?](http://puu.sh/ckeIT/4f946ab5d7.png)
And there it is. We now know we need to create a window of class "Winamp v1.x" with a title in the format of "(track number). (artist) - (song name) - Winamp" that will then receive our SendMessages.

## The Code
I haven't faintest clue how to C and I lost my debian VM to a rabid pack of wild hard drive failure, so I'ma go get that going. 