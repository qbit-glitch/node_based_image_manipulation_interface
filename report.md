# Image Editor Project Report

## Overview
This project is a comprehensive image editing application built using C++, OpenCV, and ImGui. It provides a user-friendly interface for performing various image manipulation operations with real-time preview capabilities.

## Core Features

### 1. Basic Image Operations

#### 1.1 Image Loading and Saving
- Supports loading images in various formats (BMP, JPG, JPEG, PNG, TIF, TIFF)
- Provides file dialogs for both opening and saving images
- Automatically handles file format selection and validation

#### 1.2 Image Reset and Undo
- Maintains a history stack of image states (up to 20 states)
- Allows undoing operations to previous states
- Provides a complete reset to original image functionality

### 2. Color and Channel Operations

#### 2.1 Grayscale Conversion
- Converts color images to grayscale using OpenCV's BGR2GRAY conversion
- Maintains 3-channel format for consistent processing

#### 2.2 Channel Splitting
- Splits color images into individual BGR channels
- Provides both grayscale and colorized views of each channel
- Real-time channel visualization

### 3. Image Transformations

#### 3.1 Brightness and Contrast
- Brightness adjustment range: -100 to 100
- Contrast adjustment range: 1% to 300%
- Algorithm:
  ```cpp
  output = (input * contrast/100.0) + brightness
  ```

#### 3.2 Rotation
- Rotates image around center point
- Angle range: 0° to 360°
- Uses OpenCV's warpAffine with rotation matrix

### 4. Filter Operations

#### 4.1 Blur Operations
- Gaussian Blur:
  - Adjustable kernel size (1-20)
  - Uses OpenCV's GaussianBlur function
- Directional Blur:
  - Custom motion blur implementation
  - Adjustable angle and intensity
  - Algorithm:
    ```cpp
    // Create motion blur kernel
    kernel = getMotionBlurKernel(size, angle)
    // Apply convolution
    filter2D(image, result, -1, kernel)
    ```

#### 4.2 Edge Detection
- Supports multiple methods:
  1. Sobel Edge Detection:
     - Adjustable kernel size (3-15, odd numbers)
     - Calculates gradient magnitude
  2. Canny Edge Detection:
     - Adjustable thresholds
     - Non-maximum suppression
     - Edge tracking by hysteresis
- Optional edge overlay with customizable color and opacity

#### 4.3 Thresholding
- Multiple threshold methods:
  1. Binary Threshold:
     - User-defined threshold value
     - Maximum value setting
  2. Adaptive Threshold:
     - Block size parameter
     - C value for mean adjustment
  3. Otsu's Method:
     - Automatic threshold calculation
     - Based on histogram analysis

### 5. Advanced Operations

#### 5.1 Image Blending
- Multiple blend modes:
  1. Normal: Simple alpha blending
  2. Multiply: Pixel-wise multiplication
  3. Screen: Inverse multiplication
  4. Overlay: Conditional blending
  5. Difference: Absolute difference
- Adjustable opacity
- Real-time preview

#### 5.2 Noise Generation
- Multiple noise types:
  1. Perlin Noise
  2. Simplex Noise
  3. Worley Noise
  4. Value Noise
  5. Fractal Brownian Motion
- Adjustable parameters:
  - Scale
  - Amplitude
  - Octaves (for FBM)
  - Persistence
  - Lacunarity
- Optional colorization and inversion

#### 5.3 Convolution Filtering
- Custom kernel support (3x3 or 5x5)
- Preset kernels:
  1. Sharpen
  2. Emboss
  3. Edge Enhance
- Adjustable scale and offset
- Real-time kernel editing

### 6. UI Features

#### 6.1 Main Interface
- Split-pane layout
- Resizable panels
- Real-time image preview
- Histogram visualization
- Channel splitter view

#### 6.2 Interactive Tools
- Crop tool with rectangle selection
- Real-time parameter adjustment
- Multiple undo levels
- File dialog integration

## Technical Implementation

### Core Components
1. Image Processing Engine:
   - OpenCV-based image manipulation
   - Efficient memory management
   - State history tracking

2. UI Framework:
   - ImGui-based interface
   - OpenGL texture handling
   - Real-time updates

3. File Management:
   - Cross-platform file dialogs
   - Format validation
   - Error handling

### Performance Considerations
- Efficient memory usage with history stack
- Optimized image processing algorithms
- Real-time preview optimization
- Texture management for large images

## Usage Guidelines

### Basic Workflow
1. Load an image using File > Open
2. Apply desired operations using the controls panel
3. Preview changes in real-time
4. Save the result using File > Save

### Best Practices
- Use undo/redo for experimentation
- Save intermediate results
- Monitor memory usage with large images
- Use appropriate file formats for output

## Future Enhancements
1. Additional filter types
2. Batch processing capabilities
3. Plugin system for custom operations
4. GPU acceleration support
5. Advanced color management
6. Layer support
7. Scripting interface

## Dependencies
- OpenCV
- ImGui
- GLFW
- OpenGL
- Standard C++ Library

## System Requirements
- C++17 compatible compiler
- OpenCV 4.x
- OpenGL 3.3+
- GLFW 3.x
- ImGui 1.89+ 