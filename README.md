# lslpub_XsensRaw
C++ programs that publishes Xsens raw IMU sensors data to a LSL stream.

## TODO
- [X] create repository
- [X] read the doc
- [X] get the serial data
- [X] process it
- [X] stream LSL
- [ ] see which raw data can be displayed

## Architecture
### INPUTS:
  - Raw Xsens data
  - from serial port/wirless port
### OUTPUTS:
  - LSL chunk/samples stream

## Installation
### Ubuntu 18
#### Requirements
None

#### Steps
##### Compile
```bash
make
```
##### Run
Connect the wirelessmaster to you pc via USB.
```bash
sudo ./lslsub_XsensRaw 
```
switch on all the sensors you want.

then press "y" on your keyboard.


### Windows 10
#### Requirements
#### Steps
