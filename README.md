# audacious_lazyusf

A half arsed implementation of kode54's lazyusf2 library to Audacious

## Configuration
It has no configurable configuration yet. The defaults are:
*  Audio HLE instead of RSP emulation
*  Default play time of 300 and d should no tags exist
*  Resampled output rate of 44100hz 


## Behaviour
As this is the newer mupen64-based core, there will be some compatbility issue with older sets that rely on the inaccurate Project64-based core.
These issues may manifest themeselves as:
*  Freezes after playing for some duration
*  Choppy output
*  Fast playing speed.

This newer core is preferred, as it implements correct Audio DMA FIFO behavior. 
This results in a correct sounding output, as the timing requires this otherwise the playing speed will be wrong.

## Building
This relies on the liblazyusf2 plugin here https://gitlab.kode54.net/kode54/lazyusf2

To build, clone that repo in the directory lazyusf2 by running `git clone https://gitlab.kode54.net/kode54/lazyusf2`
Goto the lazyusf2 directory and run `make liblazyusf.a`

Once built, go back to this directory run make && sudo make install

