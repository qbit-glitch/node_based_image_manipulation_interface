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
        //float resizeRatio = 100.0f;   // Range 10 to 300 (percentage)
    } params;

    // OpenGL texture for displaying the image
    GLuint imageTexture = 0;
    int imageWidth = 0;
    int imageHeight = 0;
    
    // Window dimensions
    int windowWidth = 1280;
    int windowHeight = 720;
    
    // UI state
    bool showDemoWindow = false;
    bool showAboutWindow = false;
    bool showHelpWindow = false;
    
    // Crop mode state
    bool isDragging = false;
    ImVec2 dragStart;
    ImVec2 dragEnd;
    
    // UI scaling factors
    float uiScale = 1.5f;  // Increase UI scale for larger components
    float fontSize = 25.0f; // Increase font size for better readability

public:
    ImageEditorGUI(const string& path = "") {
        imagePath = path;
        
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
        workingImage = historyStack[currentHistoryIndex - 1].clone();
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
        
        // // Apply resize if not 100%
        // if (params.resizeRatio != 100.0f) {
        //     double ratio = params.resizeRatio / 100.0;
        //     int newWidth = static_cast<int>(originalImage.cols * ratio);
        //     int newHeight = static_cast<int>(originalImage.rows * ratio);
        //     if (newWidth > 0 && newHeight > 0) {  // Ensure valid dimensions
        //         resize(image, image, Size(newWidth, newHeight), 0, 0, INTER_LINEAR);
        //     }
        // }
        
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
        // params.resizeRatio = 100.0f;
        
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
        
        Mat sharpeningKernel = (Mat_<float>(3, 3) << -1, -1, -1, -1, 9, -1, -1, -1, -1);
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
        
        Mat gray, edges;
        cvtColor(workingImage, gray, COLOR_BGR2GRAY);
        Canny(gray, edges, 50, 150);
        cvtColor(edges, workingImage, COLOR_GRAY2BGR);
        
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
        
        // Apply additional blur (beyond what's in trackbar)
        GaussianBlur(workingImage, workingImage, Size(15, 15), 0);
        
        // Update the texture
        updateTexture();
    }
    
    void enterCropMode() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        cropMode = true;
        
        // Initialize crop rectangle to a default size (center 50% of the image)
        cropRect.x = imageWidth / 4;
        cropRect.y = imageHeight / 4;
        cropRect.width = imageWidth / 2;
        cropRect.height = imageHeight / 2;
        
        cout << "Crop mode activated. Use the sliders to define the crop region." << endl;
    }
    
    void applyCrop() {
        if (!cropMode || workingImage.empty()) {
            cout << "Please enter crop mode first and select a region." << endl;
            return;
        }
        
        // Add current state to history before applying changes
        addToHistory(workingImage);
        
        // Normalize the rectangle (ensure width and height are positive)
        cropRect = normalizeRect(cropRect);
        
        // Check if rectangle is valid
        if (cropRect.width > 0 && cropRect.height > 0 && 
            cropRect.x >= 0 && cropRect.y >= 0 && 
            cropRect.x + cropRect.width <= workingImage.cols && 
            cropRect.y + cropRect.height <= workingImage.rows) {
            
            // Crop the image
            Mat cropped = workingImage(cropRect).clone();
            
            // Update original and working images
            originalImage = cropped.clone();
            workingImage = cropped.clone();
            
            // Exit crop mode
            cropMode = false;
            
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
            cout << "Crop operation canceled." << endl;
        }
    }
    
    // Helper function to normalize rectangle (ensure positive width/height)
    Rect normalizeRect(const Rect& rect) {
        Rect result;
        result.x = min(rect.x, rect.x + rect.width);
        result.y = min(rect.y, rect.y + rect.height);
        result.width = abs(rect.width);
        result.height = abs(rect.height);
        return result;
    }
    
    // Render the ImGui interface
    void renderUI() {
        // Set font size for better readability
        ImGui::GetIO().FontGlobalScale = uiScale;
        
        // Main window - make it fixed (non-draggable)
        ImGui::Begin("Image Editor", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        
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
        ImGui::BeginChild("ImageDisplay", ImVec2(0, 0), true);
        
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
            
            // Display the image
            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<unsigned long long>(imageTexture)), ImVec2(displayWidth, displayHeight));
            
            // Handle crop mode
            if (cropMode) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Crop Mode Active - Use the controls below to define crop region");
                
                // Get the image position
                ImVec2 imagePos = ImGui::GetItemRectMin();
                
                // Draw crop rectangle using ImGui drawing functions
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                
                // Convert crop rectangle from image coordinates to screen coordinates
                float scaleX = displayWidth / static_cast<float>(imageWidth);
                float scaleY = displayHeight / static_cast<float>(imageHeight);
                
                ImVec2 screenRectMin(
                    imagePos.x + cropRect.x * scaleX,
                    imagePos.y + cropRect.y * scaleY
                );
                
                ImVec2 screenRectMax(
                    imagePos.x + (cropRect.x + cropRect.width) * scaleX,
                    imagePos.y + (cropRect.y + cropRect.height) * scaleY
                );
                
                // Draw the rectangle
                draw_list->AddRect(screenRectMin, screenRectMax, IM_COL32(0, 255, 0, 255), 0.0f, 0, 2.0f);
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
            // Increase button sizes
            float buttonWidth = 150.0f * uiScale;
            float buttonHeight = 40.0f * uiScale;
            
            if (ImGui::Button("Open Image", ImVec2(buttonWidth, buttonHeight))) {
                openImageDialog();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Image", ImVec2(buttonWidth, buttonHeight))) {
                saveImageDialog();
            }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Reset", ImVec2(buttonWidth, buttonHeight))) {
                resetImage();
            }
            ImGui::SameLine();
            if (ImGui::Button("Undo", ImVec2(buttonWidth, buttonHeight))) {
                undo();
            }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Grayscale", ImVec2(buttonWidth, buttonHeight))) {
                applyGrayscale();
            }
            ImGui::SameLine();
            if (ImGui::Button("Sharpen", ImVec2(buttonWidth, buttonHeight))) {
                applySharpen();
            }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Invert", ImVec2(buttonWidth, buttonHeight))) {
                applyInvert();
            }
            ImGui::SameLine();
            if (ImGui::Button("Edge Detection", ImVec2(buttonWidth, buttonHeight))) {
                applyEdgeDetection();
            }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Blur", ImVec2(buttonWidth, buttonHeight))) {
                applyBlur();
            }
            ImGui::SameLine();
            if (ImGui::Button("Crop Mode", ImVec2(buttonWidth, buttonHeight))) {
                enterCropMode();
            }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Apply Crop", ImVec2(buttonWidth, buttonHeight))) {
                applyCrop();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel Crop", ImVec2(buttonWidth, buttonHeight))) {
                cancelCrop();
            }
        }
        
        // Crop controls (only visible in crop mode)
        if (cropMode && ImGui::CollapsingHeader("Crop Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Increase slider height
            float sliderHeight = 30.0f * uiScale;
            
            // X position slider
            if (ImGui::SliderInt("X Position", &cropRect.x, 0, imageWidth - 1, nullptr, ImGuiSliderFlags_None)) {
                // Ensure width is valid
                if (cropRect.x + cropRect.width > imageWidth) {
                    cropRect.width = imageWidth - cropRect.x;
                }
            }
            
            // Y position slider
            if (ImGui::SliderInt("Y Position", &cropRect.y, 0, imageHeight - 1, nullptr, ImGuiSliderFlags_None)) {
                // Ensure height is valid
                if (cropRect.y + cropRect.height > imageHeight) {
                    cropRect.height = imageHeight - cropRect.y;
                }
            }
            
            // Width slider
            if (ImGui::SliderInt("Width", &cropRect.width, 1, imageWidth - cropRect.x, nullptr, ImGuiSliderFlags_None)) {
                // No additional validation needed
            }
            
            // Height slider
            if (ImGui::SliderInt("Height", &cropRect.height, 1, imageHeight - cropRect.y, nullptr, ImGuiSliderFlags_None)) {
                // No additional validation needed
            }
            
            ImGui::Text("Crop Region: %d x %d at (%d, %d)", 
                       cropRect.width, cropRect.height, cropRect.x, cropRect.y);
            
            // Display crop region as a percentage of the image
            if (imageWidth > 0 && imageHeight > 0) {
                float widthPercent = (float)cropRect.width / imageWidth * 100.0f;
                float heightPercent = (float)cropRect.height / imageHeight * 100.0f;
                ImGui::Text("Size: %.1f%% x %.1f%% of image", widthPercent, heightPercent);
            }
            
            // Preset crop options
            ImGui::Text("Preset Crops:");
            ImGui::SameLine();
            if (ImGui::Button("Center 50%", ImVec2(120.0f * uiScale, 30.0f * uiScale))) {
                cropRect.x = imageWidth / 4;
                cropRect.y = imageHeight / 4;
                cropRect.width = imageWidth / 2;
                cropRect.height = imageHeight / 2;
            }
            ImGui::SameLine();
            if (ImGui::Button("Center 75%", ImVec2(120.0f * uiScale, 30.0f * uiScale))) {
                cropRect.x = imageWidth / 8;
                cropRect.y = imageHeight / 8;
                cropRect.width = imageWidth * 3 / 4;
                cropRect.height = imageHeight * 3 / 4;
            }
            ImGui::SameLine();
            if (ImGui::Button("Full Image", ImVec2(120.0f * uiScale, 30.0f * uiScale))) {
                cropRect.x = 0;
                cropRect.y = 0;
                cropRect.width = imageWidth;
                cropRect.height = imageHeight;
            }
        }
        
        // Adjustments
        if (ImGui::CollapsingHeader("Adjustments", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Increase slider height
            float sliderHeight = 30.0f * uiScale;
            
            // Brightness slider
            if (ImGui::SliderFloat("Brightness", &params.brightness, -100.0f, 100.0f, "%.1f", ImGuiSliderFlags_None)) {
                updateImage();
            }
            
            // Contrast slider
            if (ImGui::SliderFloat("Contrast", &params.contrast, 1.0f, 300.0f, "%.1f", ImGuiSliderFlags_None)) {
                updateImage();
            }
            
            // Blur size slider
            if (ImGui::SliderFloat("Blur Size", &params.blurSize, 0.0f, 15.0f, "%.1f", ImGuiSliderFlags_None)) {
                updateImage();
            }
            
            // Rotation slider
            if (ImGui::SliderFloat("Rotation", &params.rotationAngle, 0.0f, 360.0f, "%.1f", ImGuiSliderFlags_None)) {
                updateImage();
            }
            
            // Resize slider
            // if (ImGui::SliderFloat("Resize %", &params.resizeRatio, 10.0f, 300.0f, "%.1f")) {
            //     updateImage();
            // }
        }
        
        // Image info
        if (ImGui::CollapsingHeader("Image Info", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!workingImage.empty()) {
                ImGui::Text("Dimensions: %d x %d", workingImage.cols, workingImage.rows);
                ImGui::Text("Channels: %d", workingImage.channels());
                ImGui::Text("Type: %s", workingImage.type() == CV_8UC3 ? "8-bit BGR" : "Other");
            } else {
                ImGui::Text("No image loaded");
            }
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
        
        // Setup style
        ImGui::StyleColorsDark();
        
        // Increase font size
        ImFontConfig fontConfig;
        fontConfig.OversampleH = 3;
        fontConfig.OversampleV = 3;
        io.Fonts->AddFontDefault(&fontConfig);
        
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
            
            // Get current window size
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            
            // Update window dimensions if changed
            if (width != windowWidth || height != windowHeight) {
                windowWidth = width;
                windowHeight = height;
            }
            
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

    ImageEditorGUI editor(imagePath);
    
    // Start the application
    editor.run();
    
    return 0;
}