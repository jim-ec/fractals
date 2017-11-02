# fractals
Fractal implementations

This project is divided into two sub projects:
- console-fractals
- vulkan-fractals

## Build

The console-fractals project is platform independant, but the vulkan project depends windows, currently.
Just run the top-level `CMakeLists.txt` file.

Tip: `cmake --build . ` builds the cmake project without explicitly using the build system.

## console-fractals

Contained inside the `console-fractals` directory.

Prints a fractal, either Mandelbrot or Julia to the console.
The image is pretty large, therefore it's recommended to set
a small font size, like 4 or 5 points.

Hit enter to see the fractal with in incremented iteration count.

To tune the fractal you see, you have to go into the source code, namely `main.cpp`.

## vulkan-fractals

Contained inside the `console-fractals` directory.

Displays fractals using the Vulkan API. Exact features have not been set, yet.
