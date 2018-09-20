# DogTricks

A library and tools for making your satellite radio dog do tricks.

## Dependencies

* tclap

## Building

    mkdir build
    cmake ..
    make -j`nproc`
    ./src/dogtricks

## Command Line Interface

The binary ``dogtricks`` accepts a variety of arguments to control various
features of the radio.

Obtain a complete documentation by passing ``--help``. Here are some flags that
can be passed to the tool:

    USAGE: 
    
       ./src/dogtricks  [--set_channel <channel>] [--log_signal_strength]
                        [--reset] [--path <path>] [--] [--version] [-h]
    
    
    Where: 
    
       --set_channel <channel>
         sets the channel that the radio is decoding
    
       --log_signal_strength
         logs the current signal strength
    
       --reset
         reset the radio before executing other commands
    
       --path <path>
         the path of the serial device to communicate with
    
       --,  --ignore_rest
         Ignores the rest of the labeled arguments following this flag.
    
       --version
         Displays version information and exits.
    
       -h,  --help
         Displays usage information and exits.
    
    
       A tool for making satellite radio dogs do tricks.

## Hardware

This tool may work with any radio that suports an RS-232 interface. A USB to
RS-232 cable is recommended.

It has been tested with the following models:

* SDST5V1

## Affiliation

This project has no affiliation to any commercial entity. There is no warranty
that this project is fit for any purpose.
