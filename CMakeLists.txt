cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nrf_noise_measure)

target_sources(app PRIVATE src/main.c src/si24r1.c src/spectrum.c
    src/spectrum_display.c src/button_filter.c)
