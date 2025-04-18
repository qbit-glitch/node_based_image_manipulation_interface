#include <opencv2/opencv.hpp>
#include <string>
#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include <GLFW/glfw3.h>
#include "external/imgui/imgui.h"
#include "external/imgui/backends/imgui_impl_glfw.h"
#include "external/imgui/backends/imgui_impl_opengl3.h"
#include <algorithm> // Add this for std::clamp
#include <sys/stat.h> // Add this for stat functionality

// For file dialogs
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

using namespace std;
using namespace cv;

// Function to open a file dialog for opening files
string openFileDialog() {
#ifdef _WIN32
    char filename[MAX_PATH];
    
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "Image Files\0*.bmp;*.jpg;*.jpeg;*.png;*.tif;*.tiff\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrTitle = "Select an Image";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileName(&ofn)) {
        return string(filename);
    }
    return "";
#else
    // For Linux systems, use zenity for a graphical file dialog
    string command = "zenity --file-selection --title=\"Select an Image\" --file-filter=\"Image Files | *.jpg *.jpeg *.png *.bmp *.tif *.tiff\" --file-filter=\"All Files | *.*\"";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        cerr << "Failed to open file dialog. Falling back to terminal input." << endl;
        string path;
        cout << "Enter the path to the image file: ";
        getline(cin, path);
        return path;
    }
    
    char buffer[1024];
    string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    // Remove trailing newline if present
    if (!result.empty() && result[result.length() - 1] == '\n') {
        result.erase(result.length() - 1);
    }
    
    pclose(pipe);
    return result;
#endif
}

// Function to open a file dialog for saving files
string saveFileDialog() {
#ifdef _WIN32
    char filename[MAX_PATH];
    
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "PNG Files\0*.png\0JPEG Files\0*.jpg;*.jpeg\0BMP Files\0*.bmp\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrTitle = "Save Image As";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileName(&ofn)) {
        return string(filename);
    }
    return "";
#else
    // For Linux systems, use zenity for a graphical file dialog
    string command = "zenity --file-selection --save --title=\"Save Image As\" --file-filter=\"PNG Files | *.png\" --file-filter=\"JPEG Files | *.jpg *.jpeg\" --file-filter=\"BMP Files | *.bmp\" --file-filter=\"All Files | *.*\"";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        cerr << "Failed to open file dialog. Falling back to terminal input." << endl;
        string path;
        cout << "Enter the path to save the image file: ";
        getline(cin, path);
        
        // Ensure the path has a valid extension
        if (path.find('.') == string::npos) {
            path += ".png"; // Default to PNG if no extension is provided
        }
        return path;
    }
    
    char buffer[1024];
    string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    // Remove trailing newline if present
    if (!result.empty() && result[result.length() - 1] == '\n') {
        result.erase(result.length() - 1);
    }
    
    pclose(pipe);
    
    // Ensure the path has a valid extension
    if (!result.empty() && result.find('.') == string::npos) {
        result += ".png"; // Default to PNG if no extension is provided
    }
    
    return result;
#endif
}

class ImageEditorGUI {
private:
    Mat originalImage;     // Store original image for reset
    Mat workingImage;      // Current working image
    string imagePath;
    bool cropMode = false;
    Rect cropRect;
    Point startPoint;
    Mat tempImage;
    
    // History stack for undo operations
    vector<Mat> historyStack;
    size_t currentHistoryIndex = 0;
    const size_t maxHistorySize = 20;  // Limit history size to prevent excessive memory usage
    
    // Parameters for adjustments
    struct {
        float brightness = 0.0f;      // Range -100 to 100
        float contrast = 100.0f;      // Range 0 to 300 (100 is normal)
        float blurSize = 0.0f;        // Range 0 to 15
        float rotationAngle = 0.0f;   // Range 0 to 360
        // float resizeRatio = 100.0f;   // Range 10 to 300 (percentage)
        
        // Advanced blur parameters
        float gaussianBlurRadius = 5.0f;  // Range 1 to 20
        float directionalBlurAngle = 0.0f; // Range 0 to 360
        bool useDirectionalBlur = false;   // Toggle between uniform and directional blur
        
        // Threshold parameters
        int thresholdValue = 128;     // Range 0 to 255
        int thresholdMaxValue = 255;  // Maximum value for binary threshold
        int adaptiveBlockSize = 11;   // Block size for adaptive threshold (must be odd)
        int adaptiveC = 2;            // Constant subtracted from mean for adaptive threshold
        int thresholdMethod = 0;      // 0: Binary, 1: Adaptive, 2: Otsu
        
        // Edge detection parameters
        int edgeDetectionMethod = 0;  // 0: Sobel, 1: Canny
        int sobelKernelSize = 3;      // Kernel size for Sobel (must be odd)
        int cannyThreshold1 = 50;     // First threshold for Canny
        int cannyThreshold2 = 150;    // Second threshold for Canny
        bool overlayEdges = false;    // Whether to overlay edges on original image
        float edgeColor[3] = {0.0f, 1.0f, 0.0f}; // Green color for edges (BGR)
        float edgeOpacity = 0.7f;     // Opacity of edge overlay (0-1)
        
        // Blend parameters
        int blendMode = 0;            // 0: Normal, 1: Multiply, 2: Screen, 3: Overlay, 4: Difference
        float blendOpacity = 1.0f;    // Range 0 to 1
        string blendImagePath = "";    // Path to the second image for blending
        
        // Noise parameters
        int noiseType = 0;            // 0: Perlin, 1: Simplex, 2: Worley, 3: Value, 4: Fractal Brownian Motion
        float noiseScale = 10.0f;     // Scale of the noise (higher = finer detail)
        float noiseAmplitude = 1.0f;  // Amplitude of the noise (0-1)
        int noiseOctaves = 4;         // Number of octaves for FBM (1-8)
        float noisePersistence = 0.5f; // Persistence for FBM (0-1)
        float noiseLacunarity = 2.0f; // Lacunarity for FBM (1-4)
        bool noiseInvert = false;     // Invert the noise pattern
        bool noiseColorize = false;   // Apply color to the noise
        float noiseColor[3] = {0.0f, 0.5f, 1.0f}; // Color for noise (BGR)

        // Convolution parameters
        int kernelSize = 3;           // 3x3 or 5x5
        float kernel[25] = {0};       // Max size for 5x5 kernel
        float kernelScale = 1.0f;     // Scale factor for kernel values
        float kernelOffset = 0.0f;    // Offset added to result
        int currentPreset = 0;        // 0: Custom, 1: Sharpen, 2: Emboss, 3: Edge Enhance
    } params;
    
    // OpenGL texture for displaying the image
    GLuint imageTexture = 0;
    int imageWidth = 0;
    int imageHeight = 0;
    
    // Window dimensions
    int windowWidth = 2400;
    int windowHeight = 1800;
    
    // UI state
    bool showDemoWindow = false;
    bool showAboutWindow = false;
    bool showHelpWindow = false;
    bool showBlurOptions = false;  // For advanced blur options window
    bool showThresholdOptions = false;  // For threshold options window
    bool showEdgeDetectionOptions = false;  // For edge detection options window
    bool showBlendOptions = false;  // For blend options window
    bool showBrightnessOptions = false;  // For brightness options window
    bool showContrastOptions = false;  // For contrast options window
    bool showRotationOptions = false;  // For rotation options window
    
    // Resizable pane heights
    float imagePaneHeight = 600.0f;
    float propertiesPaneHeight = 600.0f;
    float imageInfoPaneHeight = 200.0f;
    
    // Crop mode state
    bool isDragging = false;
    ImVec2 dragStart;
    ImVec2 dragEnd;
    
    // Channel splitter state
    bool showChannelSplitter = false;
    bool showGrayscaleChannels = false;
    vector<Mat> splitChannels;
    vector<GLuint> channelTextures;
    vector<Mat> grayscaleChannels;
    vector<GLuint> grayscaleTextures;
    
    // Active operation for properties pane
    enum ActiveOperation {
        NONE,
        BRIGHTNESS,
        CONTRAST,
        ROTATION,
        BLUR,
        THRESHOLD,
        EDGE_DETECTION,
        BLEND,
        NOISE,
        CONVOLUTION
    };
    ActiveOperation activeOperation = NONE;

    // Initialize default kernels
    void initializeDefaultKernels() {
        // Initialize kernel with zeros
        std::fill(params.kernel, params.kernel + 25, 0.0f);
        
        // Set default 3x3 identity kernel
        if (params.kernelSize == 3) {
            params.kernel[0] = 0.0f; params.kernel[1] = 0.0f; params.kernel[2] = 0.0f;
            params.kernel[3] = 0.0f; params.kernel[4] = 1.0f; params.kernel[5] = 0.0f;
            params.kernel[6] = 0.0f; params.kernel[7] = 0.0f; params.kernel[8] = 0.0f;
        } else {
            // 5x5 identity kernel
            params.kernel[12] = 1.0f; // Center pixel for 5x5
        }
    }

    void applyPresetKernel() {
        std::fill(params.kernel, params.kernel + 25, 0.0f);
        params.kernelScale = 1.0f;
        params.kernelOffset = 0.0f;
        
        switch (params.currentPreset) {
            case 1: // Sharpen
                params.kernelSize = 3;
                params.kernel[0] = 0.0f; params.kernel[1] = -1.0f; params.kernel[2] = 0.0f;
                params.kernel[3] = -1.0f; params.kernel[4] = 5.0f; params.kernel[5] = -1.0f;
                params.kernel[6] = 0.0f; params.kernel[7] = -1.0f; params.kernel[8] = 0.0f;
                break;
                
            case 2: // Emboss
                params.kernelSize = 3;
                params.kernel[0] = -2.0f; params.kernel[1] = -1.0f; params.kernel[2] = 0.0f;
                params.kernel[3] = -1.0f; params.kernel[4] = 1.0f; params.kernel[5] = 1.0f;
                params.kernel[6] = 0.0f; params.kernel[7] = 1.0f; params.kernel[8] = 2.0f;
                params.kernelScale = 1.0f;
                params.kernelOffset = 128.0f;
                break;
                
            case 3: // Edge Enhance
                params.kernelSize = 3;
                params.kernel[0] = 0.0f; params.kernel[1] = -1.0f; params.kernel[2] = 0.0f;
                params.kernel[3] = -1.0f; params.kernel[4] = 4.0f; params.kernel[5] = -1.0f;
                params.kernel[6] = 0.0f; params.kernel[7] = -1.0f; params.kernel[8] = 0.0f;
                break;
        }
    }

    void applyConvolution() {
        if (!workingImage.data) return;
        
        Mat result;
        int kSize = params.kernelSize;
        Mat kernelMat = Mat(kSize, kSize, CV_32F);
        
        // Copy kernel values to Mat
        for (int i = 0; i < kSize; i++) {
            for (int j = 0; j < kSize; j++) {
                kernelMat.at<float>(i, j) = params.kernel[i * kSize + j] * params.kernelScale;
            }
        }
        
        // Apply convolution
        Mat temp;
        if (workingImage.channels() == 1) {
            filter2D(workingImage, temp, -1, kernelMat, Point(-1, -1), params.kernelOffset);
        } else {
            vector<Mat> channels;
            split(workingImage, channels);
            
            vector<Mat> results;
            for (auto& channel : channels) {
                Mat channelResult;
                filter2D(channel, channelResult, -1, kernelMat, Point(-1, -1), params.kernelOffset);
                results.push_back(channelResult);
            }
            
            merge(results, temp);
        }
        
        // Add to history and update
        addToHistory(workingImage);
        temp.copyTo(workingImage);
        updateTexture();
    }

public:
    ImageEditorGUI(const string& path = "") {
        imagePath = path;
        
        // Initialize default kernels
        initializeDefaultKernels();
        
        // If path is provided, load the image
        if (!path.empty()) {
            loadImage(path);
        }
    }
    
    ~ImageEditorGUI() {
        // Clean up OpenGL texture
        if (imageTexture != 0) {
            glDeleteTextures(1, &imageTexture);
        }
        
        // Clean up channel textures
        for (auto& texture : channelTextures) {
            if (texture != 0) {
                glDeleteTextures(1, &texture);
            }
        }
        
        // Clean up grayscale textures
        for (auto& texture : grayscaleTextures) {
            if (texture != 0) {
                glDeleteTextures(1, &texture);
            }
        }
    }
    
    void loadImage(const string& path) {
        imagePath = path;
        originalImage = imread(path, IMREAD_COLOR);
        
        if (originalImage.empty()) {
            cerr << "Error: Could not open or find the image: " << path << endl;
            return;
        }
        
        workingImage = originalImage.clone();
        
        // Update image dimensions
        imageWidth = workingImage.cols;
        imageHeight = workingImage.rows;
        
        // Create or update OpenGL texture
        updateTexture();
        
        // Clear history and add the original image as the first state
        clearHistory();
        addToHistory(originalImage);
    }
    
    // Add current image state to history
    void addToHistory(const Mat& image) {
        // If we're not at the end of the history, remove all states after the current one
        if (currentHistoryIndex < historyStack.size()) {
            historyStack.resize(currentHistoryIndex);
        }
        
        // Add the new state
        historyStack.push_back(image.clone());
        currentHistoryIndex = historyStack.size();
        
        // Limit history size
        if (historyStack.size() > maxHistorySize) {
            historyStack.erase(historyStack.begin());
            currentHistoryIndex--;
        }
    }
    
    // Clear history
    void clearHistory() {
        historyStack.clear();
        currentHistoryIndex = 0;
    }
    
    // Undo the last operation
    bool undo() {
        if (currentHistoryIndex <= 1) {
            cout << "Nothing to undo." << endl;
            return false;
        }
        
        currentHistoryIndex--;
        workingImage = historyStack[currentHistoryIndex].clone();
        updateTexture();
        return true;
    }
    
    // Check if undo is available
    bool canUndo() const {
        return currentHistoryIndex > 1;
    }
    
    void updateTexture() {
        if (workingImage.empty()) return;
        
        // Convert OpenCV Mat to OpenGL texture
        if (imageTexture == 0) {
            glGenTextures(1, &imageTexture);
        }
        
        glBindTexture(GL_TEXTURE_2D, imageTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        // OpenCV uses BGR format, we need to convert to RGB for OpenGL
        Mat rgbImage;
        cvtColor(workingImage, rgbImage, COLOR_BGR2RGB);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbImage.cols, rgbImage.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbImage.data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    
    // Apply all current transformations to the image
    void applyTransformations(Mat& image) {
        if (image.empty()) return;  // Skip if image is empty
        
        // Apply rotation if not 0
        if (params.rotationAngle != 0.0f) {
            Point2f center(image.cols / 2.0f, image.rows / 2.0f);
            Mat rotMat = getRotationMatrix2D(center, params.rotationAngle, 1.0);
            warpAffine(image, image, rotMat, image.size());
        }
        
        // Apply brightness and contrast
        double alpha = params.contrast / 100.0;
        int beta = static_cast<int>(params.brightness);
        image.convertTo(image, -1, alpha, beta);
        
        // Apply blur if greater than 0
        if (params.blurSize > 0.0f) {
            int blurSize = static_cast<int>(params.blurSize) * 2 + 1;
            GaussianBlur(image, image, Size(blurSize, blurSize), 0);
        }
    }
    
    // Update the image with current parameters
    void updateImage() {
        if (originalImage.empty()) return;  // Skip if no image loaded
        
        // Start with original image
        Mat processedImage = originalImage.clone();
        
        // Apply all transformations
        applyTransformations(processedImage);
        
        // Update the working image
        workingImage = processedImage.clone();
        
        // Add to history
        addToHistory(workingImage);
        
        // Update the texture
        updateTexture();
    }
    
    // Button handlers
    void openImageDialog() {
        string path = ::openFileDialog();
        if (!path.empty()) {
            loadImage(path);
        }
    }
    
    void saveImageDialog() {
        if (workingImage.empty()) {
            cout << "No image to save." << endl;
            return;
        }
        
        string path = ::saveFileDialog();
        if (!path.empty()) {
            // Ensure the path has a valid extension
            if (path.find('.') == string::npos) {
                path += ".png"; // Default to PNG if no extension is provided
            }
            
            bool success = imwrite(path, workingImage);
            if (success) {
                cout << "Image saved successfully to " << path << endl;
            } else {
                cerr << "Failed to save image to " << path << endl;
            }
        }
    }
    
    void resetImage() {
        if (originalImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        // Reset image to original
        workingImage = originalImage.clone();
        
        // Reset parameters
        params.brightness = 0.0f;
        params.contrast = 100.0f;
        params.blurSize = 0.0f;
        params.rotationAngle = 0.0f;
        
        // Exit crop mode if active
        cropMode = false;
        
        // Add to history
        addToHistory(workingImage);
        
        // Update the texture
        updateTexture();
    }
    
    void applyGrayscale() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        // Add current state to history before applying changes
        addToHistory(workingImage);
        
        Mat gray;
        cvtColor(workingImage, gray, COLOR_BGR2GRAY);
        cvtColor(gray, workingImage, COLOR_GRAY2BGR);  // Convert back to 3 channels
        
        // Update the texture
        updateTexture();
    }
    
    void applySharpen() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        // Add current state to history before applying changes
        addToHistory(workingImage);
        
        Mat sharpeningKernel = (Mat_<float>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
        filter2D(workingImage, workingImage, workingImage.depth(), sharpeningKernel);
        
        // Update the texture
        updateTexture();
    }
    
    void applyInvert() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        // Add current state to history before applying changes
        addToHistory(workingImage);
        
        bitwise_not(workingImage, workingImage);
        
        // Update the texture
        updateTexture();
    }
    
    void applyEdgeDetection() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        // Add current state to history before applying changes
        addToHistory(workingImage);
        
        // Convert to grayscale if not already
        Mat grayImage;
        if (workingImage.channels() == 3) {
            cvtColor(workingImage, grayImage, COLOR_BGR2GRAY);
        } else {
            grayImage = workingImage.clone();
        }
        
        // Apply edge detection based on selected method
        Mat edges;
        
        if (params.edgeDetectionMethod == 0) { // Sobel
            // Ensure kernel size is odd
            int kernelSize = params.sobelKernelSize;
            if (kernelSize % 2 == 0) kernelSize++;
            
            // Apply Sobel edge detection
            Mat gradX, gradY, absGradX, absGradY;
            
            // Gradient X
            Sobel(grayImage, gradX, CV_16S, 1, 0, kernelSize);
            convertScaleAbs(gradX, absGradX);
            
            // Gradient Y
            Sobel(grayImage, gradY, CV_16S, 0, 1, kernelSize);
            convertScaleAbs(gradY, absGradY);
            
            // Total gradient
            addWeighted(absGradX, 0.5, absGradY, 0.5, 0, edges);
            
        } else if (params.edgeDetectionMethod == 1) { // Canny
            // Apply Canny edge detection
            Canny(grayImage, edges, params.cannyThreshold1, params.cannyThreshold2);
        }
        
        // If overlay is enabled, blend the edges with the original image
        if (params.overlayEdges) {
            // Convert edges to BGR if it's grayscale
            Mat edgesBGR;
            if (edges.channels() == 1) {
                cvtColor(edges, edgesBGR, COLOR_GRAY2BGR);
            } else {
                edgesBGR = edges.clone();
            }
            
            // Create a colored version of the edges
            Mat coloredEdges;
            edgesBGR.copyTo(coloredEdges);
            
            // Set the color of the edges
            for (int y = 0; y < coloredEdges.rows; y++) {
                for (int x = 0; x < coloredEdges.cols; x++) {
                    if (edges.at<uchar>(y, x) > 0) {
                        coloredEdges.at<Vec3b>(y, x)[0] = static_cast<uchar>(params.edgeColor[0] * 255); // B
                        coloredEdges.at<Vec3b>(y, x)[1] = static_cast<uchar>(params.edgeColor[1] * 255); // G
                        coloredEdges.at<Vec3b>(y, x)[2] = static_cast<uchar>(params.edgeColor[2] * 255); // R
                    }
                }
            }
            
            // Blend the colored edges with the original image
            Mat originalBGR = workingImage.clone();
            addWeighted(originalBGR, 1.0 - params.edgeOpacity, coloredEdges, params.edgeOpacity, 0, workingImage);
        } else {
            // Just use the edges as the result
            if (edges.channels() == 1) {
        cvtColor(edges, workingImage, COLOR_GRAY2BGR);
            } else {
                workingImage = edges.clone();
            }
        }
        
        // Update the texture
        updateTexture();
    }
    
    void applyBlur() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        // Add current state to history before applying changes
        addToHistory(workingImage);
        
        // Apply blur based on selected options
        if (params.useDirectionalBlur) {
            // Directional blur (motion blur)
            // Convert angle to radians
            float angleRad = params.directionalBlurAngle * CV_PI / 180.0f;
            
            // Calculate kernel size based on radius (must be odd)
            int kernelSize = static_cast<int>(params.gaussianBlurRadius) * 2 + 1;
            
            // Create a motion blur kernel
            Mat kernel = getMotionBlurKernel(kernelSize, angleRad);
            
            // Apply the kernel
            filter2D(workingImage, workingImage, -1, kernel);
        } else {
            // Gaussian blur
            // Calculate kernel size based on radius (must be odd)
            int kernelSize = static_cast<int>(params.gaussianBlurRadius) * 2 + 1;
            
            // Apply Gaussian blur
            GaussianBlur(workingImage, workingImage, Size(kernelSize, kernelSize), 0);
        }
        
        // Update the texture
        updateTexture();
    }
    
    // Helper function to create a motion blur kernel
    Mat getMotionBlurKernel(int size, float angle) {
        // Create a kernel of the specified size
        Mat kernel = Mat::zeros(size, size, CV_32F);
        
        // Calculate the center of the kernel
        int center = size / 2;
        
        // Calculate the direction vector
        float dx = cos(angle);
        float dy = sin(angle);
        
        // Fill the kernel with values along the direction vector
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                // Calculate the distance from the center
                float x = i - center;
                float y = j - center;
                
                // Calculate the dot product with the direction vector
                float dot = x * dx + y * dy;
                
                // Calculate the perpendicular distance
                float perp = abs(x * dy - y * dx);
                
                // If the point is close to the line, set the kernel value
                if (perp < 1.0f && dot >= -size/2 && dot <= size/2) {
                    kernel.at<float>(i, j) = 1.0f;
                }
            }
        }
        
        // Normalize the kernel
        float kernelSum = cv::sum(kernel)[0];
        if (kernelSum > 0) {
            kernel /= kernelSum;
        }
        
        return kernel;
    }
    
    void enterCropMode() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        cropMode = true;
        isDragging = false;
        cout << "Crop mode activated. Draw a rectangle on the image." << endl;
    }
    
    void applyCrop() {
        if (!cropMode || workingImage.empty()) {
            cout << "Please enter crop mode first and select a region." << endl;
            return;
        }
        
        // Add current state to history before applying changes
        addToHistory(workingImage);
        
        // Ensure the crop rectangle is within image bounds
        cropRect.x = std::max(0, std::min(cropRect.x, workingImage.cols - 1));
        cropRect.y = std::max(0, std::min(cropRect.y, workingImage.rows - 1));
        cropRect.width = std::min(cropRect.width, workingImage.cols - cropRect.x);
        cropRect.height = std::min(cropRect.height, workingImage.rows - cropRect.y);
        
        // Check if rectangle is valid
        if (cropRect.width > 0 && cropRect.height > 0) {
            // Create a deep copy of the cropped region
            Mat cropped = workingImage(cropRect).clone();
            
            // Update working image with the cropped region
            workingImage = cropped.clone();
            
            // Update image dimensions
            imageWidth = workingImage.cols;
            imageHeight = workingImage.rows;
            
            // Exit crop mode
            cropMode = false;
            isDragging = false;
            
            // Update the texture
            updateTexture();
            cout << "Image cropped successfully." << endl;
        } else {
            cout << "Invalid crop region. Please try again." << endl;
        }
    }
    
    void cancelCrop() {
        if (cropMode) {
            cropMode = false;
            isDragging = false;
            // Restore the original image display
            if (!workingImage.empty()) {
                updateTexture();
            }
            cout << "Crop operation canceled." << endl;
        }
    }
    
    // Helper function to normalize rectangle (ensure positive width/height)
    Rect normalizeRect(const Rect& rect) {
        Rect result;
        result.x = std::min(rect.x, rect.x + rect.width);
        result.y = std::min(rect.y, rect.y + rect.height);
        result.width = std::abs(rect.width);
        result.height = std::abs(rect.height);
        return result;
    }
    
    // Split image into channels
    void splitImageChannels() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        // Clear previous channels
        splitChannels.clear();
        grayscaleChannels.clear();
        
        // Split the image into BGR channels
        split(workingImage, splitChannels);
        
        // Create colored versions of each channel
        for (int i = 0; i < splitChannels.size(); i++) {
            Mat colorChannel = Mat::zeros(splitChannels[i].size(), CV_8UC3);
            vector<Mat> channels(3, Mat::zeros(splitChannels[i].size(), CV_8UC1));
            channels[i] = splitChannels[i];  // Put the channel in its correct position (B=0, G=1, R=2)
            merge(channels, colorChannel);
            grayscaleChannels.push_back(colorChannel);
        }
        
        // Update textures for each channel
        updateChannelTextures();
        
        // Set active operation to NONE to show channels in properties panel
        activeOperation = NONE;
    }
    
    // Update textures for split channels
    void updateChannelTextures() {
        // Clean up previous textures
        for (auto& texture : channelTextures) {
            if (texture != 0) {
                glDeleteTextures(1, &texture);
                texture = 0;
            }
        }
        channelTextures.clear();
        
        for (auto& texture : grayscaleTextures) {
            if (texture != 0) {
                glDeleteTextures(1, &texture);
                texture = 0;
            }
        }
        grayscaleTextures.clear();
        
        // Create new textures for each channel
        for (const auto& channel : splitChannels) {
            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            // Convert to RGB for OpenGL
            Mat rgbChannel;
            cvtColor(channel, rgbChannel, COLOR_GRAY2RGB);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbChannel.cols, rgbChannel.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbChannel.data);
            glBindTexture(GL_TEXTURE_2D, 0);
            
            channelTextures.push_back(texture);
        }
        
        // Create new textures for colored versions
        for (const auto& colorChannel : grayscaleChannels) {
            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            // Convert BGR to RGB for OpenGL
            Mat rgbChannel;
            cvtColor(colorChannel, rgbChannel, COLOR_BGR2RGB);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbChannel.cols, rgbChannel.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbChannel.data);
            glBindTexture(GL_TEXTURE_2D, 0);
            
            grayscaleTextures.push_back(texture);
        }
    }
    
    // Apply threshold to the image
    void applyThreshold() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        // Add current state to history before applying changes
        addToHistory(workingImage);
        
        // Convert to grayscale if not already
        Mat grayImage;
        if (workingImage.channels() == 3) {
            cvtColor(workingImage, grayImage, COLOR_BGR2GRAY);
        } else {
            grayImage = workingImage.clone();
        }
        
        // Apply threshold based on selected method
        Mat thresholdedImage;
        
        // Ensure block size is odd (for adaptive threshold)
        int blockSize = params.adaptiveBlockSize;
        if (blockSize % 2 == 0) blockSize++;
        
        switch (params.thresholdMethod) {
            case 0: // Binary threshold
                threshold(grayImage, thresholdedImage, params.thresholdValue, params.thresholdMaxValue, THRESH_BINARY);
                break;
                
            case 1: // Adaptive threshold
                adaptiveThreshold(grayImage, thresholdedImage, params.thresholdMaxValue, 
                                 ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, blockSize, params.adaptiveC);
                break;
                
            case 2: // Otsu threshold
                threshold(grayImage, thresholdedImage, 0, params.thresholdMaxValue, THRESH_BINARY | THRESH_OTSU);
                break;
        }
        
        // Convert back to BGR for display
        if (thresholdedImage.channels() == 1) {
            cvtColor(thresholdedImage, workingImage, COLOR_GRAY2BGR);
        } else {
            workingImage = thresholdedImage.clone();
        }
        
        // Update the texture
        updateTexture();
    }
    
    // Calculate histogram for the current image
    vector<vector<int>> calculateHistogram() {
        vector<vector<int>> histogram(3, vector<int>(256, 0)); // For BGR channels
        
        if (workingImage.empty()) {
            return histogram;
        }
        
        // For grayscale images, use only the first channel
        if (workingImage.channels() == 1) {
            for (int y = 0; y < workingImage.rows; y++) {
                for (int x = 0; x < workingImage.cols; x++) {
                    int value = workingImage.at<uchar>(y, x);
                    histogram[0][value]++;
                }
            }
            return histogram;
        }
        
        // For color images, calculate histogram for each channel
        for (int y = 0; y < workingImage.rows; y++) {
            for (int x = 0; x < workingImage.cols; x++) {
                Vec3b pixel = workingImage.at<Vec3b>(y, x);
                histogram[0][pixel[0]]++; // B
                histogram[1][pixel[1]]++; // G
                histogram[2][pixel[2]]++; // R
            }
        }
        
        return histogram;
    }
    
    // Apply blend operation to the image
    void applyBlend() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        if (params.blendImagePath.empty()) {
            cout << "No second image selected for blending." << endl;
            return;
        }
        
        // Load the second image
        Mat blendImage = imread(params.blendImagePath);
        if (blendImage.empty()) {
            cout << "Failed to load the blend image: " << params.blendImagePath << endl;
            return;
        }
        
        // Add current state to history before applying changes
        addToHistory(workingImage);
        
        // Resize blend image to match the working image size if needed
        if (blendImage.size() != workingImage.size()) {
            resize(blendImage, blendImage, workingImage.size(), 0, 0, INTER_LINEAR);
        }
        
        // Apply the selected blend mode
        Mat result;
        
        switch (params.blendMode) {
            case 0: // Normal
                // Simple alpha blending
                addWeighted(workingImage, 1.0 - params.blendOpacity, blendImage, params.blendOpacity, 0, result);
                break;
                
            case 1: // Multiply
                // Multiply blend mode: result = a * b / 255
                result = Mat::zeros(workingImage.size(), workingImage.type());
                for (int y = 0; y < workingImage.rows; y++) {
                    for (int x = 0; x < workingImage.cols; x++) {
                        Vec3b a = workingImage.at<Vec3b>(y, x);
                        Vec3b b = blendImage.at<Vec3b>(y, x);
                        
                        // Apply multiply blend mode
                        Vec3b c;
                        c[0] = (a[0] * b[0]) / 255;
                        c[1] = (a[1] * b[1]) / 255;
                        c[2] = (a[2] * b[2]) / 255;
                        
                        // Apply opacity
                        result.at<Vec3b>(y, x) = a * (1.0 - params.blendOpacity) + c * params.blendOpacity;
                    }
                }
                break;
                
            case 2: // Screen
                // Screen blend mode: result = 255 - (255 - a) * (255 - b) / 255
                result = Mat::zeros(workingImage.size(), workingImage.type());
                for (int y = 0; y < workingImage.rows; y++) {
                    for (int x = 0; x < workingImage.cols; x++) {
                        Vec3b a = workingImage.at<Vec3b>(y, x);
                        Vec3b b = blendImage.at<Vec3b>(y, x);
                        
                        // Apply screen blend mode
                        Vec3b c;
                        c[0] = 255 - ((255 - a[0]) * (255 - b[0])) / 255;
                        c[1] = 255 - ((255 - a[1]) * (255 - b[1])) / 255;
                        c[2] = 255 - ((255 - a[2]) * (255 - b[2])) / 255;
                        
                        // Apply opacity
                        result.at<Vec3b>(y, x) = a * (1.0 - params.blendOpacity) + c * params.blendOpacity;
                    }
                }
                break;
                
            case 3: // Overlay
                // Overlay blend mode: if a < 128 then 2*a*b/255 else 255-2*(255-a)*(255-b)/255
                result = Mat::zeros(workingImage.size(), workingImage.type());
                for (int y = 0; y < workingImage.rows; y++) {
                    for (int x = 0; x < workingImage.cols; x++) {
                        Vec3b a = workingImage.at<Vec3b>(y, x);
                        Vec3b b = blendImage.at<Vec3b>(y, x);
                        
                        // Apply overlay blend mode
                        Vec3b c;
                        for (int i = 0; i < 3; i++) {
                            if (a[i] < 128) {
                                c[i] = 2 * a[i] * b[i] / 255;
                            } else {
                                c[i] = 255 - 2 * (255 - a[i]) * (255 - b[i]) / 255;
                            }
                        }
                        
                        // Apply opacity
                        result.at<Vec3b>(y, x) = a * (1.0 - params.blendOpacity) + c * params.blendOpacity;
                    }
                }
                break;
                
            case 4: // Difference
                // Difference blend mode: result = |a - b|
                result = Mat::zeros(workingImage.size(), workingImage.type());
                for (int y = 0; y < workingImage.rows; y++) {
                    for (int x = 0; x < workingImage.cols; x++) {
                        Vec3b a = workingImage.at<Vec3b>(y, x);
                        Vec3b b = blendImage.at<Vec3b>(y, x);
                        
                        // Apply difference blend mode
                        Vec3b c;
                        c[0] = abs(a[0] - b[0]);
                        c[1] = abs(a[1] - b[1]);
                        c[2] = abs(a[2] - b[2]);
                        
                        // Apply opacity
                        result.at<Vec3b>(y, x) = a * (1.0 - params.blendOpacity) + c * params.blendOpacity;
                    }
                }
                break;
        }
        
        // Update the working image
        workingImage = result.clone();
        
        // Update the texture
        updateTexture();
    }
    
    // Apply noise to the image
    void applyNoise() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        // Add current state to history before applying changes
        addToHistory(workingImage);
        
        // Create a noise pattern
        Mat noisePattern = Mat::zeros(workingImage.size(), CV_32F);
        
        // Generate noise based on selected type
        switch (params.noiseType) {
            case 0: // Perlin noise
                generatePerlinNoise(noisePattern, params.noiseScale);
                break;
            case 1: // Simplex noise
                generateSimplexNoise(noisePattern, params.noiseScale);
                break;
            case 2: // Worley noise
                generateWorleyNoise(noisePattern, params.noiseScale);
                break;
            case 3: // Value noise
                generateValueNoise(noisePattern, params.noiseScale);
                break;
            case 4: // Fractal Brownian Motion
                generateFBMNoise(noisePattern, params.noiseScale, params.noiseOctaves, 
                                params.noisePersistence, params.noiseLacunarity);
                break;
        }
        
        // Normalize noise to 0-1 range
        normalize(noisePattern, noisePattern, 0, 1, NORM_MINMAX);
        
        // Invert if needed
        if (params.noiseInvert) {
            noisePattern = 1.0 - noisePattern;
        }
        
        // Apply amplitude
        noisePattern *= params.noiseAmplitude;
        
        // Convert noise to BGR if colorize is enabled
        Mat noiseBGR;
        if (params.noiseColorize) {
            noiseBGR = Mat::zeros(workingImage.size(), CV_8UC3);
            for (int y = 0; y < noisePattern.rows; y++) {
                for (int x = 0; x < noisePattern.cols; x++) {
                    float value = noisePattern.at<float>(y, x);
                    noiseBGR.at<Vec3b>(y, x)[0] = static_cast<uchar>(params.noiseColor[0] * value * 255); // B
                    noiseBGR.at<Vec3b>(y, x)[1] = static_cast<uchar>(params.noiseColor[1] * value * 255); // G
                    noiseBGR.at<Vec3b>(y, x)[2] = static_cast<uchar>(params.noiseColor[2] * value * 255); // R
                }
            }
        } else {
            // Convert to grayscale
            noisePattern.convertTo(noiseBGR, CV_8UC1, 255.0);
            cvtColor(noiseBGR, noiseBGR, COLOR_GRAY2BGR);
        }
        
        // Blend with original image
        Mat result;
        addWeighted(workingImage, 1.0 - params.noiseAmplitude, noiseBGR, params.noiseAmplitude, 0, result);
        
        // Update the working image
        workingImage = result.clone();
        
        // Update the texture
        updateTexture();
    }
    
    // Generate Perlin noise
    void generatePerlinNoise(Mat& noise, float scale) {
        // Simple implementation of Perlin noise
        for (int y = 0; y < noise.rows; y++) {
            for (int x = 0; x < noise.cols; x++) {
                float nx = x / scale;
                float ny = y / scale;
                
                // Simple 2D Perlin noise approximation
                float value = 0.5f * (1.0f + sin(nx) * cos(ny));
                noise.at<float>(y, x) = value;
            }
        }
    }
    
    // Generate Simplex noise
    void generateSimplexNoise(Mat& noise, float scale) {
        // Simple implementation of Simplex noise
        for (int y = 0; y < noise.rows; y++) {
            for (int x = 0; x < noise.cols; x++) {
                float nx = x / scale;
                float ny = y / scale;
                
                // Simple 2D Simplex noise approximation
                float value = 0.5f * (1.0f + sin(nx + ny) * cos(nx - ny));
                noise.at<float>(y, x) = value;
            }
        }
    }
    
    // Generate Worley noise
    void generateWorleyNoise(Mat& noise, float scale) {
        // Simple implementation of Worley noise
        vector<Point2f> points;
        int numPoints = static_cast<int>(noise.rows * noise.cols / (scale * scale));
        
        // Generate random points
        for (int i = 0; i < numPoints; i++) {
            float x = static_cast<float>(rand()) / RAND_MAX * noise.cols;
            float y = static_cast<float>(rand()) / RAND_MAX * noise.rows;
            points.push_back(Point2f(x, y));
        }
        
        // Calculate distance to nearest point
        for (int y = 0; y < noise.rows; y++) {
            for (int x = 0; x < noise.cols; x++) {
                float minDist = FLT_MAX;
                for (const auto& p : points) {
                    float dx = x - p.x;
                    float dy = y - p.y;
                    float dist = sqrt(dx*dx + dy*dy);
                    minDist = min(minDist, dist);
                }
                
                // Normalize distance
                noise.at<float>(y, x) = minDist / (noise.rows * 0.5f);
            }
        }
    }
    
    // Generate Value noise
    void generateValueNoise(Mat& noise, float scale) {
        // Simple implementation of Value noise
        for (int y = 0; y < noise.rows; y++) {
            for (int x = 0; x < noise.cols; x++) {
                float nx = x / scale;
                float ny = y / scale;
                
                // Simple 2D Value noise approximation
                float value = 0.5f * (1.0f + sin(nx * ny));
                noise.at<float>(y, x) = value;
            }
        }
    }
    
    // Generate Fractal Brownian Motion noise
    void generateFBMNoise(Mat& noise, float scale, int octaves, float persistence, float lacunarity) {
        Mat tempNoise = Mat::zeros(noise.size(), CV_32F);
        float amplitude = 1.0f;
        float frequency = 1.0f / scale;
        float maxValue = 0.0f;
        
        // Generate base noise
        generatePerlinNoise(tempNoise, scale);
        
        // Initialize result
        noise = tempNoise.clone() * amplitude;
        maxValue = amplitude;
        
        // Add octaves
        for (int i = 1; i < octaves; i++) {
            amplitude *= persistence;
            frequency *= lacunarity;
            
            // Generate noise at this octave
            generatePerlinNoise(tempNoise, scale / frequency);
            
            // Add to result
            noise += tempNoise * amplitude;
            maxValue += amplitude;
        }
        
        // Normalize
        noise /= maxValue;
    }
    
    // Render the ImGui interface
    void renderUI() {
        // Main window
        ImGui::Begin("Image Editor", nullptr, 
            cropMode ? (ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove) : ImGuiWindowFlags_MenuBar);
        
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open Image", "Ctrl+O")) {
                    openImageDialog();
                }
                if (ImGui::MenuItem("Save Image", "Ctrl+S")) {
                    saveImageDialog();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Esc")) {
                    // Exit application
                    glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo())) {
                    undo();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Reset", "Ctrl+R")) {
                    resetImage();
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Demo Window", nullptr, &showDemoWindow);
                ImGui::MenuItem("Channel Splitter", nullptr, &showChannelSplitter);
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) {
                    showAboutWindow = true;
                }
                if (ImGui::MenuItem("Help")) {
                    showHelpWindow = true;
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMenuBar();
        }
        
        // Main content area
        ImGui::Columns(2, "MainColumns", true);
        
        // Left column: Image display
        ImGui::BeginChild("ImageDisplay", ImVec2(0, 0), true, 
            cropMode ? ImGuiWindowFlags_NoMove : 0);
        
        // Display the image
        if (imageTexture != 0) {
            // Calculate aspect ratio
            float aspectRatio = static_cast<float>(imageWidth) / static_cast<float>(imageHeight);
            
            // Calculate display size while maintaining aspect ratio
            float displayWidth = ImGui::GetContentRegionAvail().x;
            float displayHeight = displayWidth / aspectRatio;
            
            // If the calculated height is too large, scale based on height instead
            if (displayHeight > ImGui::GetContentRegionAvail().y) {
                displayHeight = ImGui::GetContentRegionAvail().y;
                displayWidth = displayHeight * aspectRatio;
            }
            
            // Center the image
            float xPos = (ImGui::GetContentRegionAvail().x - displayWidth) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xPos);
            
            // Store image position and size for crop calculations
            ImVec2 imagePos = ImGui::GetCursorScreenPos();
            ImVec2 imageSize = ImVec2(displayWidth, displayHeight);
            
            // Display the image
            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<unsigned long long>(imageTexture)), imageSize);
            
            // Handle crop mode
            if (cropMode) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Crop Mode Active - Draw a rectangle on the image");
                
                // Get mouse position relative to window
                ImVec2 mousePos = ImGui::GetMousePos();
                ImVec2 windowPos = ImGui::GetWindowPos();
                
                // Calculate mouse position relative to image
                float mouseX = mousePos.x - imagePos.x;
                float mouseY = mousePos.y - imagePos.y;
                
                // Check if mouse is over the image
                bool isMouseOverImage = mouseX >= 0 && mouseX < imageSize.x && 
                                     mouseY >= 0 && mouseY < imageSize.y;
                
                if (isMouseOverImage) {
                    // Convert mouse position to image coordinates
                    float relativeX = mouseX / imageSize.x;
                    float relativeY = mouseY / imageSize.y;
                    
                    // Handle mouse events
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        isDragging = true;
                        dragStart = ImVec2(relativeX * imageWidth, relativeY * imageHeight);
                        dragEnd = dragStart;
                    } 
                    else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && isDragging) {
                        dragEnd = ImVec2(relativeX * imageWidth, relativeY * imageHeight);
                        
                        // Update crop rectangle
                        cropRect.x = static_cast<int>(std::min(dragStart.x, dragEnd.x));
                        cropRect.y = static_cast<int>(std::min(dragStart.y, dragEnd.y));
                        cropRect.width = static_cast<int>(std::abs(dragEnd.x - dragStart.x));
                        cropRect.height = static_cast<int>(std::abs(dragEnd.y - dragStart.y));
                        
                        // Draw the crop rectangle
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        
                        // Convert image coordinates back to screen coordinates for drawing
                        ImVec2 rectMin = ImVec2(
                            imagePos.x + (std::min(dragStart.x, dragEnd.x) / imageWidth) * imageSize.x,
                            imagePos.y + (std::min(dragStart.y, dragEnd.y) / imageHeight) * imageSize.y
                        );
                        ImVec2 rectMax = ImVec2(
                            imagePos.x + (std::max(dragStart.x, dragEnd.x) / imageWidth) * imageSize.x,
                            imagePos.y + (std::max(dragStart.y, dragEnd.y) / imageHeight) * imageSize.y
                        );
                        
                        // Draw filled rectangle with semi-transparent color
                        draw_list->AddRectFilled(rectMin, rectMax, IM_COL32(255, 255, 255, 50));
                        // Draw rectangle border
                        draw_list->AddRect(rectMin, rectMax, IM_COL32(0, 255, 0, 255), 0.0f, ImDrawFlags_None, 2.0f);
                    } 
                    else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                        isDragging = false;
                    }
                }
                
                // Add crop control buttons below the image
                ImGui::Spacing();
                ImGui::Spacing();
                if (ImGui::Button("Apply Crop", ImVec2(180, 50))) {
                    applyCrop();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel Crop", ImVec2(180, 50))) {
                    cancelCrop();
                }
            }
        } else {
            ImGui::Text("No image loaded. Use File > Open Image to load an image.");
        }
        
        ImGui::EndChild();
        
        ImGui::NextColumn();
        
        // Right column: Controls
        ImGui::BeginChild("Controls", ImVec2(0, 0), true);
        
        // Image operations
        if (ImGui::CollapsingHeader("Image Operations", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Create a more dynamic button layout
            float windowWidth = ImGui::GetContentRegionAvail().x;
            int buttonsPerRow = static_cast<int>(windowWidth / 250); // Increased from 200 to 250 for wider buttons
            if (buttonsPerRow < 1) buttonsPerRow = 1;
            
            // Define all buttons with their labels and actions
            struct ButtonInfo {
                const char* label;
                std::function<void()> action;
            };
            
            std::vector<ButtonInfo> buttons = {
                {"Open Image", [this]() { openImageDialog(); }},
                {"Save Image", [this]() { saveImageDialog(); }},
                {"Reset", [this]() { resetImage(); }},
                {"Undo", [this]() { undo(); }},
                {"Grayscale", [this]() { applyGrayscale(); }},
                {"Sharpen", [this]() { applySharpen(); }},
                {"Invert", [this]() { applyInvert(); }},
                {"Brightness", [this]() { activeOperation = BRIGHTNESS; }},
                {"Contrast", [this]() { activeOperation = CONTRAST; }},
                {"Rotation", [this]() { activeOperation = ROTATION; }},
                {"Blur", [this]() { activeOperation = BLUR; }},
                {"Threshold", [this]() { activeOperation = THRESHOLD; }},
                {"Edge Detection", [this]() { activeOperation = EDGE_DETECTION; }},
                {"Blend", [this]() { activeOperation = BLEND; }},
                {"Noise", [this]() { activeOperation = NOISE; }},
                {"Convolution", [this]() { activeOperation = CONVOLUTION; }},
                {"Crop Image", [this]() { enterCropMode(); }},
                {"Split Channels", [this]() { splitImageChannels(); }}
            };
            
            // Render buttons in a grid with increased spacing and size
            for (size_t i = 0; i < buttons.size(); i++) {
                // Add vertical spacing between rows
                if (i > 0 && i % buttonsPerRow == 0) {
                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::Spacing(); // Triple spacing for more vertical separation
                }
                
                if (i > 0 && i % buttonsPerRow != 0) {
                    ImGui::SameLine(0, 25.0f); // Increased horizontal spacing between buttons from 20 to 25
                }
                
                // Calculate button width to fit the available space
                float buttonWidth = (windowWidth - (buttonsPerRow - 1) * 25.0f) / buttonsPerRow;
                if (ImGui::Button(buttons[i].label, ImVec2(buttonWidth - 5.0f, 50.0f))) { // Reduced padding from 10 to 5
                    buttons[i].action();
                }
            }
            
            // Add extra spacing after all buttons
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();
        }
        
        // Properties Pane - Resizable
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        
        // Properties header with resize handle
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        bool propertiesOpen = ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen);
        
        // Add a resize handle for properties pane
        ImGui::Button("##PropertiesResizeHandle", ImVec2(-1, 8));
        if (ImGui::IsItemActive()) {
            propertiesPaneHeight = std::max(100.0f, propertiesPaneHeight + ImGui::GetIO().MouseDelta.y);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        
        ImGui::PopStyleVar();
        
        // Properties content
        if (propertiesOpen) {
            ImGui::BeginChild("PropertiesContent", ImVec2(0, propertiesPaneHeight), true);
            
            if (activeOperation == NONE) {
                // Display channel splitter in properties when no other operation is active
                if (!splitChannels.empty()) {
                    ImGui::Text("Channel Splitter");
                    ImGui::Separator();
                    
                    // Toggle between regular and grayscale views
                    ImGui::Checkbox("Show Color Channels", &showGrayscaleChannels);
                    
                    ImGui::Spacing();
                    
                    // Get the current textures to display
                    const vector<GLuint>& displayTextures = showGrayscaleChannels ? grayscaleTextures : channelTextures;
                    
                    // Calculate the available width for the channel display
                    float availableWidth = ImGui::GetContentRegionAvail().x;
                    float channelWidth = availableWidth;
                    float aspectRatio = static_cast<float>(workingImage.cols) / static_cast<float>(workingImage.rows);
                    float channelHeight = channelWidth / aspectRatio;
                    
                    // Display each channel
                    const char* channelNames[] = { "Blue Channel", "Green Channel", "Red Channel" };
                    for (size_t i = 0; i < displayTextures.size(); ++i) {
                        ImGui::Text("%s", channelNames[i]);
                        
                        // Center the image
                        float xPos = (availableWidth - channelWidth) * 0.5f;
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xPos);
                        
                        // Display the channel
                        ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<unsigned long long>(displayTextures[i])), 
                                   ImVec2(channelWidth, channelHeight));
                        
                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();
                    }
                } else {
                    ImGui::Text("Select an operation to view its properties.");
                }
            } else {
                // Display properties based on active operation
                
                // Declare variables outside the switch statement to avoid crossing initialization
                const char* thresholdMethods[] = { "Binary", "Adaptive", "Otsu" };
                const char* edgeMethods[] = { "Sobel", "Canny" };
                const char* blendModes[] = { "Normal", "Multiply", "Screen", "Overlay", "Difference" };
                const char* noiseTypes[] = { "Perlin", "Simplex", "Worley", "Value", "Fractal Brownian Motion" };
                const char* kernelSizes[] = { "3x3", "5x5" };
                const char* presets[] = { "Custom", "Sharpen", "Emboss", "Edge Enhance" };
                
                vector<vector<int>> histogram;
                int maxCount = 0;
                ImDrawList* draw_list;
                ImVec2 canvas_pos;
                ImVec2 canvas_size;
                float bar_width;
                int currentSize;
                float cellWidth;
                
                // Calculate histogram if needed for threshold
                if (activeOperation == THRESHOLD) {
                    histogram = calculateHistogram();
                    
                    // Find maximum value for scaling
                    for (const auto& channel : histogram) {
                        for (int count : channel) {
                            maxCount = max(maxCount, count);
                        }
                    }
                }
                
                switch (activeOperation) {
                    case BRIGHTNESS:
                        ImGui::Text("Brightness Properties");
                        ImGui::Separator();
                        
                        // Brightness slider
                        if (ImGui::SliderFloat("Brightness", &params.brightness, -100.0f, 100.0f, "%.1f")) {
                            updateImage();
            }
            
            ImGui::Spacing();
            
                        // Apply button
                        if (ImGui::Button("Apply Brightness", ImVec2(220, 50))) {
                            updateImage();
                        }
                        break;
                        
                    case CONTRAST:
                        ImGui::Text("Contrast Properties");
                        ImGui::Separator();
                        
                        // Contrast slider
                        if (ImGui::SliderFloat("Contrast", &params.contrast, 1.0f, 300.0f, "%.1f")) {
                            updateImage();
            }
            
            ImGui::Spacing();
            
                        // Apply button
                        if (ImGui::Button("Apply Contrast", ImVec2(220, 50))) {
                            updateImage();
                        }
                        break;
                        
                    case ROTATION:
                        ImGui::Text("Rotation Properties");
                        ImGui::Separator();
                        
                        // Rotation slider
                        if (ImGui::SliderFloat("Rotation Angle", &params.rotationAngle, 0.0f, 360.0f, "%.1f")) {
                            updateImage();
            }
            
            ImGui::Spacing();
            
                        // Apply button
                        if (ImGui::Button("Apply Rotation", ImVec2(220, 50))) {
                            updateImage();
                        }
                        break;
                        
                    case BLUR:
                        ImGui::Text("Blur Properties");
                        ImGui::Separator();
                        
                        // Toggle between uniform and directional blur
                        ImGui::Checkbox("Use Directional Blur", &params.useDirectionalBlur);
                        
                        ImGui::Spacing();
                        
                        // Gaussian blur radius slider
                        if (ImGui::SliderFloat("Blur Radius", &params.gaussianBlurRadius, 1.0f, 20.0f, "%.1f")) {
                            // Ensure the value is an integer
                            params.gaussianBlurRadius = round(params.gaussianBlurRadius);
                        }
                        
                        // Directional blur angle slider (only shown if directional blur is enabled)
                        if (params.useDirectionalBlur) {
                            if (ImGui::SliderFloat("Blur Angle", &params.directionalBlurAngle, 0.0f, 360.0f, "%.1f")) {
                                // Update the preview if needed
                            }
                            
                            // Visual representation of the angle
                            ImGui::Text("Blur Direction:");
                            
                            // Create a simple visualization of the angle
                            draw_list = ImGui::GetWindowDrawList();
                            canvas_pos = ImGui::GetCursorScreenPos();
                            canvas_pos.x += 50;
                            canvas_pos.y += 50;
                            
                            float radius = 40.0f;
                            float angle_rad = params.directionalBlurAngle * CV_PI / 180.0f;
                            
                            // Draw a circle
                            draw_list->AddCircle(canvas_pos, radius, IM_COL32(200, 200, 200, 255), 32, 1.0f);
                            
                            // Draw the direction line
                            ImVec2 direction_end;
                            direction_end.x = canvas_pos.x + radius * cos(angle_rad);
                            direction_end.y = canvas_pos.y + radius * sin(angle_rad);
                            draw_list->AddLine(canvas_pos, direction_end, IM_COL32(255, 0, 0, 255), 2.0f);
                            
                            // Add some space after the visualization
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 100);
                        }
                        
                        ImGui::Spacing();
                        
                        // Apply button
                        if (ImGui::Button("Apply Blur", ImVec2(220, 50))) {
                applyBlur();
            }
                        break;
                        
                    case THRESHOLD:
                        ImGui::Text("Threshold Properties");
                        ImGui::Separator();
                        
                        // Threshold method selection
                        ImGui::Combo("Threshold Method", &params.thresholdMethod, thresholdMethods, IM_ARRAYSIZE(thresholdMethods));
                        
                        ImGui::Spacing();
                        
                        // Parameters based on selected method
                        if (params.thresholdMethod == 0) { // Binary threshold
                            if (ImGui::SliderInt("Threshold Value", &params.thresholdValue, 0, 255)) {
                                // Update preview if needed
                            }
                            ImGui::SliderInt("Max Value", &params.thresholdMaxValue, 0, 255);
                        } else if (params.thresholdMethod == 1) { // Adaptive threshold
                            // Ensure block size is odd
                            int blockSize = params.adaptiveBlockSize;
                            if (blockSize % 2 == 0) blockSize++;
                            
                            if (ImGui::SliderInt("Block Size", &blockSize, 3, 99)) {
                                // Ensure it stays odd
                                params.adaptiveBlockSize = (blockSize % 2 == 0) ? blockSize + 1 : blockSize;
                            }
                            ImGui::SliderInt("C Value", &params.adaptiveC, -10, 10);
                            ImGui::SliderInt("Max Value", &params.thresholdMaxValue, 0, 255);
                        } else if (params.thresholdMethod == 2) { // Otsu threshold
                            ImGui::SliderInt("Max Value", &params.thresholdMaxValue, 0, 255);
                            ImGui::Text("Otsu's method automatically determines the optimal threshold value.");
            }
            
            ImGui::Spacing();
            
                        // Histogram display
                        ImGui::Text("Image Histogram");
                        
                        // Display histogram
                        ImGui::BeginChild("Histogram", ImVec2(ImGui::GetContentRegionAvail().x, 150), true);
                        
                        draw_list = ImGui::GetWindowDrawList();
                        canvas_pos = ImGui::GetCursorScreenPos();
                        canvas_size = ImGui::GetContentRegionAvail();
                        
                        // Draw background
                        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), 
                                                IM_COL32(30, 30, 30, 255));
                        
                        // Draw histogram bars
                        bar_width = canvas_size.x / 256.0f;
                        
                        // For binary threshold, show grayscale histogram
                        if (params.thresholdMethod == 0) {
                            // Draw threshold line
                            float threshold_x = canvas_pos.x + params.thresholdValue * bar_width;
                            draw_list->AddLine(ImVec2(threshold_x, canvas_pos.y), 
                                              ImVec2(threshold_x, canvas_pos.y + canvas_size.y), 
                                              IM_COL32(255, 0, 0, 255), 2.0f);
                            
                            // Draw histogram bars
                            for (int i = 0; i < 256; i++) {
                                float bar_height = (histogram[0][i] / (float)maxCount) * canvas_size.y;
                                draw_list->AddRectFilled(
                                    ImVec2(canvas_pos.x + i * bar_width, canvas_pos.y + canvas_size.y - bar_height),
                                    ImVec2(canvas_pos.x + (i + 1) * bar_width, canvas_pos.y + canvas_size.y),
                                    IM_COL32(200, 200, 200, 255)
                                );
                            }
                        } else {
                            // For other methods, show color histogram
                            for (int i = 0; i < 256; i++) {
                                float bar_height_b = (histogram[0][i] / (float)maxCount) * canvas_size.y / 3.0f;
                                float bar_height_g = (histogram[1][i] / (float)maxCount) * canvas_size.y / 3.0f;
                                float bar_height_r = (histogram[2][i] / (float)maxCount) * canvas_size.y / 3.0f;
                                
                                // Blue channel
                                draw_list->AddRectFilled(
                                    ImVec2(canvas_pos.x + i * bar_width, canvas_pos.y + canvas_size.y - bar_height_b),
                                    ImVec2(canvas_pos.x + (i + 1) * bar_width, canvas_pos.y + canvas_size.y),
                                    IM_COL32(0, 0, 255, 255)
                                );
                                
                                // Green channel
                                draw_list->AddRectFilled(
                                    ImVec2(canvas_pos.x + i * bar_width, canvas_pos.y + canvas_size.y - bar_height_b - bar_height_g),
                                    ImVec2(canvas_pos.x + (i + 1) * bar_width, canvas_pos.y + canvas_size.y - bar_height_b),
                                    IM_COL32(0, 255, 0, 255)
                                );
                                
                                // Red channel
                                draw_list->AddRectFilled(
                                    ImVec2(canvas_pos.x + i * bar_width, canvas_pos.y + canvas_size.y - bar_height_b - bar_height_g - bar_height_r),
                                    ImVec2(canvas_pos.x + (i + 1) * bar_width, canvas_pos.y + canvas_size.y - bar_height_b - bar_height_g),
                                    IM_COL32(255, 0, 0, 255)
                                );
                            }
                        }
                        
                        ImGui::EndChild();
                        
                        ImGui::Spacing();
                        
                        // Apply button
                        if (ImGui::Button("Apply Threshold", ImVec2(220, 50))) {
                            applyThreshold();
                        }
                        break;
                        
                    case EDGE_DETECTION:
                        ImGui::Text("Edge Detection Properties");
                        ImGui::Separator();
                        
                        // Edge detection method selection
                        ImGui::Combo("Edge Detection Method", &params.edgeDetectionMethod, edgeMethods, IM_ARRAYSIZE(edgeMethods));
                        
                        ImGui::Spacing();
                        
                        // Parameters based on selected method
                        if (params.edgeDetectionMethod == 0) { // Sobel
                            // Ensure kernel size is odd
                            int kernelSize = params.sobelKernelSize;
                            if (kernelSize % 2 == 0) kernelSize++;
                            
                            if (ImGui::SliderInt("Kernel Size", &kernelSize, 3, 15, "%d")) {
                                // Ensure it stays odd
                                params.sobelKernelSize = (kernelSize % 2 == 0) ? kernelSize + 1 : kernelSize;
                            }
                            ImGui::Text("Note: Kernel size must be odd. Value will be adjusted if needed.");
                        } else if (params.edgeDetectionMethod == 1) { // Canny
                            ImGui::SliderInt("Threshold 1", &params.cannyThreshold1, 1, 255);
                            ImGui::SliderInt("Threshold 2", &params.cannyThreshold2, 1, 255);
                            
                            // Ensure threshold2 is greater than threshold1
                            if (params.cannyThreshold2 < params.cannyThreshold1) {
                                params.cannyThreshold2 = params.cannyThreshold1;
                            }
                        }
                        
                        ImGui::Spacing();
                        
                        // Overlay options
                        ImGui::Checkbox("Overlay Edges on Original Image", &params.overlayEdges);
                        
                        if (params.overlayEdges) {
                            ImGui::SliderFloat("Edge Opacity", &params.edgeOpacity, 0.0f, 1.0f, "%.2f");
                            
                            // Color picker for edge color
                            ImGui::Text("Edge Color (BGR):");
                            ImGui::ColorEdit3("##EdgeColor", params.edgeColor);
                        }
                        
                        ImGui::Spacing();
                        
                        // Apply button
                        if (ImGui::Button("Apply Edge Detection", ImVec2(220, 50))) {
                            applyEdgeDetection();
                        }
                        break;
                        
                    case BLEND:
                        ImGui::Text("Blend Properties");
                        ImGui::Separator();
                        
                        // Blend mode selection
                        ImGui::Combo("Blend Mode", &params.blendMode, blendModes, IM_ARRAYSIZE(blendModes));
                        
                        ImGui::Spacing();
                        
                        // Opacity slider
                        ImGui::SliderFloat("Opacity", &params.blendOpacity, 0.0f, 1.0f, "%.2f");
                        
                        ImGui::Spacing();
                        
                        // Second image selection
                        ImGui::Text("Second Image for Blending:");
                        
                        // Display current path or "None selected"
                        if (params.blendImagePath.empty()) {
                            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No image selected");
                        } else {
                            ImGui::Text("%s", params.blendImagePath.c_str());
                        }
                        
                        // Button to select second image
                        if (ImGui::Button("Select Image", ImVec2(180, 50))) {
                            string path = openFileDialog();
                            if (!path.empty()) {
                                params.blendImagePath = path;
                            }
                        }
                        
                        ImGui::Spacing();
                        
                        // Apply button
                        if (ImGui::Button("Apply Blend", ImVec2(220, 50))) {
                            applyBlend();
                        }
                        break;
                        
                    case NOISE:
                        ImGui::Text("Noise Generation Properties");
                        ImGui::Separator();
                        
                        // Noise type selection
                        ImGui::Combo("Noise Type", &params.noiseType, noiseTypes, IM_ARRAYSIZE(noiseTypes));
                        
                        ImGui::Spacing();
                        
                        // Common parameters
                        ImGui::SliderFloat("Scale", &params.noiseScale, 1.0f, 50.0f, "%.1f");
                        ImGui::SliderFloat("Amplitude", &params.noiseAmplitude, 0.0f, 1.0f, "%.2f");
                        
                        // FBM specific parameters
                        if (params.noiseType == 4) { // Fractal Brownian Motion
                            ImGui::SliderInt("Octaves", &params.noiseOctaves, 1, 8);
                            ImGui::SliderFloat("Persistence", &params.noisePersistence, 0.0f, 1.0f, "%.2f");
                            ImGui::SliderFloat("Lacunarity", &params.noiseLacunarity, 1.0f, 4.0f, "%.2f");
                        }
                        
                        ImGui::Spacing();
                        
                        // Invert option
                        ImGui::Checkbox("Invert Noise", &params.noiseInvert);
                        
                        // Colorize option
                        ImGui::Checkbox("Colorize Noise", &params.noiseColorize);
                        
                        if (params.noiseColorize) {
                            ImGui::Text("Noise Color (BGR):");
                            ImGui::ColorEdit3("##NoiseColor", params.noiseColor);
                        }
                        
                        ImGui::Spacing();
                        
                        // Apply button
                        if (ImGui::Button("Apply Noise", ImVec2(220, 50))) {
                            applyNoise();
                        }
                        break;

                    case CONVOLUTION:
                        ImGui::Text("Convolution Filter Properties");
                        ImGui::Separator();
                        
                        // Kernel size selection
                        currentSize = (params.kernelSize == 3) ? 0 : 1;
                        if (ImGui::Combo("Kernel Size", &currentSize, kernelSizes, IM_ARRAYSIZE(kernelSizes))) {
                            params.kernelSize = (currentSize == 0) ? 3 : 5;
                            // Reset kernel when size changes
                            std::fill(params.kernel, params.kernel + 25, 0.0f);
                            params.kernel[params.kernelSize * params.kernelSize / 2] = 1.0f; // Center pixel
                        }
                        
                        ImGui::Spacing();
                        
                        // Preset selection
                        if (ImGui::Combo("Preset", &params.currentPreset, presets, IM_ARRAYSIZE(presets))) {
                            applyPresetKernel();
                        }
                        
                        ImGui::Spacing();
                        
                        // Kernel matrix editor
                        if (ImGui::CollapsingHeader("Kernel Matrix", ImGuiTreeNodeFlags_DefaultOpen)) {
                            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
                            
                            cellWidth = ImGui::GetContentRegionAvail().x / (params.kernelSize + 1);
                            for (int i = 0; i < params.kernelSize; i++) {
                                for (int j = 0; j < params.kernelSize; j++) {
                                    if (j > 0) ImGui::SameLine();
                                    char label[32];
                                    snprintf(label, sizeof(label), "##K%d%d", i, j);
                                    ImGui::PushItemWidth(cellWidth);
                                    ImGui::InputFloat(label, &params.kernel[i * params.kernelSize + j], 0.0f, 0.0f, "%.3f");
                                    ImGui::PopItemWidth();
                                }
                            }
                            
                            ImGui::PopStyleVar();
                        }
                        
                        ImGui::Spacing();
                        
                        // Scale and offset controls
                        ImGui::SliderFloat("Scale", &params.kernelScale, 0.1f, 5.0f, "%.3f");
                        ImGui::SliderFloat("Offset", &params.kernelOffset, -255.0f, 255.0f, "%.1f");
                        
                        ImGui::Spacing();
                        
                        // Apply button
                        if (ImGui::Button("Apply Convolution", ImVec2(220, 50))) {
                            applyConvolution();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Reset Kernel", ImVec2(180, 50))) {
                            initializeDefaultKernels();
                        }
                        break;
                }
                
                // Add crop controls to the properties pane when in crop mode
                if (cropMode) {
                    ImGui::Separator();
                    ImGui::Text("Crop Controls");
                    ImGui::Separator();
                    
                    ImGui::Text("Draw a rectangle on the image to select the crop area.");
                    
                    ImGui::Spacing();
                    
                    // Add Apply and Cancel buttons for crop operation
                    if (ImGui::Button("Apply Crop", ImVec2(180, 50))) {
                        applyCrop();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel Crop", ImVec2(180, 50))) {
                        cancelCrop();
                    }
                }
            }
            
            ImGui::EndChild();
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Image Info - Resizable
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        
        // Image Info header with resize handle
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        bool imageInfoOpen = ImGui::CollapsingHeader("Image Info", ImGuiTreeNodeFlags_DefaultOpen);
        
        // Add a resize handle for image info pane
        ImGui::Button("##ImageInfoResizeHandle", ImVec2(-1, 8));
        if (ImGui::IsItemActive()) {
            imageInfoPaneHeight = std::max(50.0f, imageInfoPaneHeight + ImGui::GetIO().MouseDelta.y);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        
        ImGui::PopStyleVar();
        
        // Image Info content
        if (imageInfoOpen) {
            ImGui::BeginChild("ImageInfoContent", ImVec2(0, imageInfoPaneHeight), true);
            
            if (!workingImage.empty()) {
                ImGui::Text("Dimensions: %d x %d", workingImage.cols, workingImage.rows);
                ImGui::Text("Channels: %d", workingImage.channels());
                ImGui::Text("Type: %s", workingImage.type() == CV_8UC3 ? "8-bit BGR" : "Other");
                
                // Get file size and format
                if (!imagePath.empty()) {
                    // Get file size
                    struct stat st;
                    if (stat(imagePath.c_str(), &st) == 0) {
                        double fileSize = st.st_size;
                        const char* sizeUnit = "B";
                        if (fileSize >= 1024) {
                            fileSize /= 1024;
                            sizeUnit = "KB";
                            if (fileSize >= 1024) {
                                fileSize /= 1024;
                                sizeUnit = "MB";
                            }
                        }
                        ImGui::Text("File Size: %.2f %s", fileSize, sizeUnit);
                    }
                    
                    // Get file format from extension
                    size_t dotPos = imagePath.find_last_of(".");
                    if (dotPos != string::npos) {
                        string format = imagePath.substr(dotPos + 1);
                        // Convert to uppercase
                        transform(format.begin(), format.end(), format.begin(), ::toupper);
                        ImGui::Text("Format: %s", format.c_str());
                    }
                }
            } else {
                ImGui::Text("No image loaded");
            }
            
            ImGui::EndChild();
        }
        
        ImGui::EndChild();
        
        ImGui::Columns(1);
        
        ImGui::End();
        
        // Demo window
        if (showDemoWindow) {
            ImGui::ShowDemoWindow(&showDemoWindow);
        }
        
        // About window
        if (showAboutWindow) {
            ImGui::Begin("About", &showAboutWindow, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Image Editor with ImGui");
            ImGui::Text("Version 1.0");
            ImGui::Separator();
            ImGui::Text("A simple image editor using OpenCV and ImGui");
            ImGui::Text("Created by Your Name");
            ImGui::End();
        }
        
        // Help window
        if (showHelpWindow) {
            ImGui::Begin("Help", &showHelpWindow, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Image Editor Help");
            ImGui::Separator();
            ImGui::Text("File Operations:");
            ImGui::BulletText("Open Image: Load an image file");
            ImGui::BulletText("Save Image: Save the current image");
            ImGui::BulletText("Reset: Reset the image to its original state");
            ImGui::Separator();
            ImGui::Text("Image Operations:");
            ImGui::BulletText("Grayscale: Convert image to grayscale");
            ImGui::BulletText("Sharpen: Apply sharpening filter");
            ImGui::BulletText("Invert: Invert image colors");
            ImGui::BulletText("Edge Detection: Apply Canny edge detection");
            ImGui::BulletText("Blur: Apply Gaussian blur");
            ImGui::Separator();
            ImGui::Text("Crop Operations:");
            ImGui::BulletText("Crop Mode: Enter crop mode");
            ImGui::BulletText("Apply Crop: Apply the selected crop region");
            ImGui::BulletText("Cancel Crop: Cancel the crop operation");
            ImGui::End();
        }
    }
    
    // Main run loop
    void run() {
        // Initialize GLFW
        if (!glfwInit()) {
            cerr << "Failed to initialize GLFW" << endl;
            return;
        }
        
        // Create window
        GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Image Editor", nullptr, nullptr);
        if (!window) {
            cerr << "Failed to create GLFW window" << endl;
            glfwTerminate();
            return;
        }
        
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // Enable vsync
        
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        // Set up high DPI scaling
        float dpi_scale = 2.0f;
        ImGui::GetStyle().ScaleAllSizes(dpi_scale);
        
        // Load and scale fonts
        ImFontConfig font_config;
        font_config.SizePixels = 13.0f * dpi_scale; // Base font size * scale
        io.Fonts->AddFontDefault(&font_config);
        
        // Customize ImGui style for better visibility at larger scale
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(20, 20);
        style.WindowRounding = 7.0f;
        style.FramePadding = ImVec2(8, 8);
        style.FrameRounding = 5.0f;
        style.ItemSpacing = ImVec2(16, 12);
        style.ItemInnerSpacing = ImVec2(10, 8);
        style.IndentSpacing = 30.0f;
        style.ScrollbarSize = 20.0f;
        style.ScrollbarRounding = 12.0f;
        style.GrabMinSize = 8.0f;
        style.GrabRounding = 4.0f;
        
        // Increase window size to accommodate larger UI
        windowWidth = 1600;
        windowHeight = 1000;
        
        // Setup colors with slightly adjusted alpha for better visibility
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.96f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 0.60f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.26f, 0.26f, 0.50f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.26f, 0.75f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.50f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        
        // Setup style
        ImGui::StyleColorsDark();
        
        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");
        
        // If no image is loaded yet, open dialog
        if (originalImage.empty()) {
            openImageDialog();
        }
        
        // Main loop
        while (!glfwWindowShouldClose(window)) {
            // Poll and handle events
            glfwPollEvents();
            
            // Start the ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            // Render the UI
            renderUI();
            
            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            glfwSwapBuffers(window);
        }
        
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

// Main function
int main(int argc, char* argv[]) {
    string imagePath = "";

    
    // Create an instance of the editor
    ImageEditorGUI editor(imagePath);
    
    // Start the application
    editor.run();
    
    return 0;
}