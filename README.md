# Node Based Image Manipulation Interface

Node Based Image manipulation Interface is a powerful and user-friendly image manipulation application built with C++, OpenCV, and Dear ImGui. This desktop application provides a modern interface for real-time image editing and processing operations. This application allows users to load images, process them through a series of connected nodes, and save the resulting output. 

## Features

- **Basic Image Operations**
  - Load images from file system
  - Display image metadata (dimensions, file size, format)
  - Brightness and contrast adjustment
  - Rotation
  - Cropping
  - Grayscale conversion
  - Image inversion

- **Advanced Image Processing**
  - Multiple blur types (Gaussian, Directional)
  - Edge detection (Sobel, Canny)
  - Thresholding (Binary, Adaptive, Otsu)
  - Channel splitting (RGB/BGR and Grayscale)
  - Custom convolution kernels
  - Image blending with multiple modes
  - Procedural noise generation (Perlin, Simplex, Worley, Value, FBM)

- **User Interface Features**
  - Real-time preview of adjustments
  - Undo functionality (up to 20 steps)
  - File dialogs for opening and saving images
  - Customizable workspace layout
  - Channel visualization
  - Histogram display

## Dependencies

- OpenCV (4.x)
- GLFW3
- OpenGL
- Dear ImGui
- C++17 compatible compiler
- CMake (3.15 or higher)

## Building from Source

1. Clone the repository:
```bash
git clone https://github.com/yourusername/node_based_image_manipulation_interface.git
cd node_based_image_manipulation_interface
```

2. Create a build directory:
```bash
mkdir build
cd build
```

3. Configure and build the project:
```bash
cmake ..
make
```

## Usage

1. Launch the application:
```bash
./MyProject [image_path]
```

2. If no image path is provided, you can load an image using File -> Open or the file dialog.

3. Use the various tools and adjustments in the interface to modify your image:
   - Adjust brightness/contrast using sliders
   - Apply different blur types and edge detection methods
   - Use the channel splitter to analyze individual color channels
   - Apply custom convolution kernels or use presets
   - Generate and blend procedural noise patterns
   - Crop images using the interactive crop tool

4. Save your processed image using File -> Save or the save dialog.

## Controls

- Left mouse button: Interact with UI elements
- Left mouse button + drag: Draw crop region when in crop mode
- Right mouse button: Cancel current operation
- Mouse wheel: Adjust slider values
- Ctrl+Z: Undo last operation
- Ctrl+S: Save image
- Ctrl+O: Open image
- Esc: Exit crop mode or close current dialog

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.  