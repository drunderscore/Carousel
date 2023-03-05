# Carousel

An NDI source finder & viewer, made to work on Linux & Windows

## How to Build

### Linux

I recommend using `ninja` as your generator.

```
mkdir Build
cd Build
cmake -G Ninja ..
ninja
```

### Windows

I also recommend using `ninja` as your generator, but a VS Project is probably fine too.

This currently **does not build out of the box on Windows**, you will have to point CMake explicitly to GLFW headers and
libraries, and NDI libraries -- but it does build if you do so.