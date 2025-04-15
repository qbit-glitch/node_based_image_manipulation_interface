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

## Fine Grained Details about each feature : 
<br>
- **Load Images from File System**:
    - It uses `zenity` file dialog for linux and `Win32` API for Windows platform. 
    - This system was tested on Linux OS, so there may be some bugs while building from source code in Windows. Therefore advisable to test the systm on Linux.

- **Save Images to File System** :
    - Same as `Load images`, uses `zenity` for file Dialogs i.e. loading and saving images from local directories.
    - Should provide the desired filename eg: `sample.jpeg` to save in those desired formats.
    - By default if no extensions are provided, it saves as a `.png` file format.

- **Reset Image** : 
    - Resets to the original image, removing all the filters added and clearning the undo history.

- **Grayscale**:
    - Converts the image into grayscale image.
    - After converting pushes the grayscale image into the history stack for undo operations. Same is done for every single operations which can be applied on the image.
    - converts a color image to grayscale using OpenCV's `cvtColor` function with `COLOR_BGR2GRAY` conversion, then converts it back to BGR format to maintain the 3-channel structure, effectively creating a grayscale image while preserving the original image format.

- **Sharpen** : 
    - Sharpens the image.
    - Uses a 3x3 convolution kernel with a center weight of 5 and surrounding weights of -1 to sharpen the image. Uses the Laplacian Sharpening kernel.
    ```txt
    0, -1, 0
    -1, 5, -1
    0, -1, 0
    ```
    - The `filter2D` function applies this kernel to the image, which enhances edges by increasing the contrast between adjacent pixels while preserving the overall image structure.

- 

- **Invert**:
    - Inverts the image.
    - Inverts the image colors using OpenCV's `bitwise_not` function, which performs a bitwise NOT operation on each pixel value, effectively creating a negative of the original image by subtracting each color channel value from 255

- **Edge Detection**:
    - Detects edges using 2 methods : Sobel and Canny.
    - It first converts the image to grayscale if needed, then applies the selected edge detection method (Sobel with configurable kernel size or Canny with adjustable thresholds).
    - The operation also maintains an undo history by saving the image state before applying changes.

- **Blur** : 
    - Implements two types of blur effects on an image: directional (motion) blur and Gaussian blur.
    - It first checks if an image is loaded, saves the current state to history, then applies either a directional blur using a custom motion kernel at a specified angle, or a Gaussian blur with a configurable radius.
    - Finally, it updates the texture to display the blurred result.


- **Crop Mode**:
    - Crops a part of an image captured within a rectangular region.
    - When crop mode is activate the window pane becomes non-draggable i.e. we cannot drag the window as per our need when we are in crop mode. This ensures proper drawing of rectangle while being convinient to the user.
    - Functions : 
        - `enterCropMode()` activates the cropping interface,
        - `applyCrop()` handles the actual cropping by validating and applying the selected rectangular region while maintaining image bounds and history, and
        - `cancelCrop()` allows aborting the operation and restoring the original image.
    - The system includes safety checks, maintains undo history, and updates the display after each operation.


- **Split Channels**:
    - Splits the image into 3 channels of color : Red, Blue, Green which can be seen in `properties` pane.
    - At first it only renders the grayscale version of R,G,B channels but it have a toggle button to view the three different colored channel images.
    - Functions : 
        - `splitImageChannels()` splits the BGR image into individual color channels and creates both grayscale and colored versions of each channel,
        - `updateChannelTextures()` manages OpenGL textures for displaying these channels, handling texture creation, parameter setup, and proper color space conversion between BGR and RGB formats.

- **Threshold**
    - Have different thresholding methods : binary, adaptive and Otsu thresholding.
    - Displays the histogram of the image to assist with threshold selection.
    - Functions : 
        - `applyThreshold()` converts the image to grayscale and applies one of three thresholding methods (binary, adaptive, or Otsu's) with configurable parameters,
        - `calculateHistogram()` computes intensity distributions for each color channel (BGR) or grayscale values, returning a vector of 256-bin histograms that represent the frequency of each intensity value in the image.

- **Blend**:
    - Combine Two images using different `Blend` modes i.e. normal, multiplay, difference, overlay and screen modes.
    - It loads a second image, resizes it to match the first image's dimensions, and applies one of five blend modes with configurable opacity.
    - Uses pixel-by-pixel operations to combine the images according to each blend mode's mathematical formula while maintaining the original image's color space and dimensions.
    - Short overview of different modes : 
      1. Normal: Simple alpha blending using `addWeighted()` with configurable opacity between the two images.
      2. Multiply: Each pixel channel is multiplied `(a * b / 255)` to create a darker result, then blended with original using opacity.
      3. Screen: Inverts both images, multiplies them, and inverts back `(255 - (255-a) * (255-b) / 255)` for a lighter effect.
      4. Overlay: Combines Multiply and Screen based on base image intensity 
      ```txt
      if a < 128: 2ab/255, 
      else: 255-2(255-a)(255-b)/255
      ```
      5. Difference: Takes absolute difference between corresponding pixels `|a - b|` to highlight variations between images.

- **Noise**:
    - Creates procedural noise patterns using different algorithms like : Perlin, Simplex, Worley, Value and FBM.
    - It generates noise patterns using different algorithms, normalizes the noise to a 0-1 range, allows for inversion and amplitude adjustment, and has the option to colorize the noise before blending it with the original image using configurable opacity and colors.
    - Noise Modes : 
        1. Perlin Noise: Uses sine and cosine functions to create smooth, natural-looking noise patterns by interpolating between random gradients, with the scale parameter controlling the frequency of the noise.
        2. Simplex Noise: Similar to Perlin but uses a different mathematical approach with sine and cosine of combined coordinates (nx+ny and nx-ny), creating more uniform noise distribution with fewer artifacts.
        3. Worley Noise: Generates random points in space and calculates the distance to the nearest point for each pixel, creating cellular/voronoi-like patterns that can be scaled to control the cell size.
        4. Value Noise: Creates simpler noise patterns using sine of multiplied coordinates (nx*ny), producing more regular but less natural-looking patterns compared to Perlin noise.
        5. Fractal Brownian Motion (FBM): Combines multiple octaves of Perlin noise with decreasing amplitude and increasing frequency, controlled by persistence and lacunarity parameters to create more complex, natural-looking patterns.


## UI Rendered using ImGUI
- ImGUI is a bloat-free graphical user interface library for C++ that focuses on enabling fast iterations and empowering programmers to create content creation tools, light-weight applications, game engines and visualization/ debugging tools.

- Official Repo : https://github.com/ocornut/imgui
- It has self documenting code present in the file : `imgui_demo.cpp` in the root directory.

- In the project, I have implemented a comprehensive ImGui-based user interface for the image editor, featuring a main window with menu bar, two-column layout (image display and controls), and resizable property panes.

- It handles various operations through interactive controls including image loading/saving, editing operations (brightness, contrast, blur, etc.), crop mode with visual rectangle selection, and real-time previews with histograms and visualizations.

- **Key UI Components**:
  1.  Menu Bar: File operations (Open/Save), Edit (Undo/Reset), View (Demo/Channel Splitter), and Help options
  2. Image Display: Centered image view with aspect ratio preservation and crop mode visualization. The image display pane is horizontally resizable.
  3. Controls Panel: Grid of operation buttons and resizable property panes with operation-specific controls
  4. Properties Pane: Dynamic controls for each operation (sliders, color pickers, kernel editors) with real-time updates. It's vertically resizable and scrollable giving a smooth user-experience.
  5. Image Info: Displays image metadata (dimensions, channels, file size, format) in a resizable panel. 

- The UI is responsive to the layout meaning the buttons as well as the image spans only to the area provided to them. It adjusts it's layout and behaviour to fit and function well on different screen sizes.


## Screenshots





## Dependencies

- OpenCV (4.*)
- GLFW3
- OpenGL
- Dear ImGui
- C++17 compatible compiler
- CMake (3.15 or higher)



## Building from Source

1. Clone the repository:
```bash
git clone https://github.com/qbit-glitch/node_based_image_manipulation_interface.git
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
./MyProject
```
or 
```bash
./MyProject [image-path]
```

2. If no image path is provided, we can load an image using File -> Open or the file dialog.

3. Use the various tools and adjustments in the interface to modify your image:
   - Adjust brightness/contrast using sliders
   - Apply different blur types and edge detection methods
   - Use the channel splitter to analyze individual color channels
   - Apply custom convolution kernels or use presets
   - Generate and blend procedural noise patterns
   - Crop images using the interactive crop tool

4. Save your processed image using File -> Save or the save dialog.


## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.  