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
    
    // Parameters for adjustments
    struct {
        float brightness = 0.0f;      // Range -100 to 100
        float contrast = 100.0f;      // Range 0 to 300 (100 is normal)
        float blurSize = 0.0f;        // Range 0 to 15
        float rotationAngle = 0.0f;   // Range 0 to 360
        // float resizeRatio = 100.0f;   // Range 10 to 300 (percentage) // Bug in this part of the code. Rectify it.
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
        
        // Update the texture
        updateTexture();
    }
    
    void applyGrayscale() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
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
        
        bitwise_not(workingImage, workingImage);
        
        // Update the texture
        updateTexture();
    }
    
    void applyEdgeDetection() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
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
        cout << "Crop mode activated. Draw a rectangle on the image." << endl;
    }
    
    void applyCrop() {
        if (!cropMode || workingImage.empty()) {
            cout << "Please enter crop mode first and select a region." << endl;
            return;
        }
        
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
        // Main window
        ImGui::Begin("Image Editor", nullptr, ImGuiWindowFlags_MenuBar);
        
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
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Crop Mode Active - Draw a rectangle on the image");
                
                // Handle mouse input for crop selection
                ImVec2 mousePos = ImGui::GetMousePos();
                ImVec2 imagePos = ImGui::GetItemRectMin();
                
                // Check if mouse is over the image
                if (ImGui::IsMouseHoveringRect(imagePos, ImVec2(imagePos.x + displayWidth, imagePos.y + displayHeight))) {
                    // Convert mouse position to image coordinates
                    float scaleX = static_cast<float>(imageWidth) / displayWidth;
                    float scaleY = static_cast<float>(imageHeight) / displayHeight;
                    
                    int imgX = static_cast<int>((mousePos.x - imagePos.x) * scaleX);
                    int imgY = static_cast<int>((mousePos.y - imagePos.y) * scaleY);
                    
                    // Handle mouse events
                    if (ImGui::IsMouseClicked(0)) {
                        isDragging = true;
                        dragStart = ImVec2(static_cast<float>(imgX), static_cast<float>(imgY));
                        dragEnd = dragStart;
                    } else if (ImGui::IsMouseDragging(0) && isDragging) {
                        dragEnd = ImVec2(static_cast<float>(imgX), static_cast<float>(imgY));
                        
                        // Update crop rectangle
                        cropRect.x = static_cast<int>(min(dragStart.x, dragEnd.x));
                        cropRect.y = static_cast<int>(min(dragStart.y, dragEnd.y));
                        cropRect.width = static_cast<int>(abs(dragEnd.x - dragStart.x));
                        cropRect.height = static_cast<int>(abs(dragEnd.y - dragStart.y));
                        
                        // Create a temporary image with the crop rectangle drawn
                        tempImage = workingImage.clone();
                        rectangle(tempImage, cropRect, Scalar(0, 255, 0), 2);
                        updateTexture();
                    } else if (ImGui::IsMouseReleased(0) && isDragging) {
                        isDragging = false;
                    }
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
            if (ImGui::Button("Open Image", ImVec2(120, 30))) {
                openImageDialog();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Image", ImVec2(120, 30))) {
                saveImageDialog();
            }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Reset", ImVec2(120, 30))) {
                resetImage();
            }
            ImGui::SameLine();
            if (ImGui::Button("Grayscale", ImVec2(120, 30))) {
                applyGrayscale();
            }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Sharpen", ImVec2(120, 30))) {
                applySharpen();
            }
            ImGui::SameLine();
            if (ImGui::Button("Invert", ImVec2(120, 30))) {
                applyInvert();
            }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Edge Detection", ImVec2(120, 30))) {
                applyEdgeDetection();
            }
            ImGui::SameLine();
            if (ImGui::Button("Blur", ImVec2(120, 30))) {
                applyBlur();
            }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Crop Mode", ImVec2(120, 30))) {
                enterCropMode();
            }
            ImGui::SameLine();
            if (ImGui::Button("Apply Crop", ImVec2(120, 30))) {
                applyCrop();
            }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Cancel Crop", ImVec2(120, 30))) {
                cancelCrop();
            }
        }
        
        // Adjustments
        if (ImGui::CollapsingHeader("Adjustments", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Brightness slider
            if (ImGui::SliderFloat("Brightness", &params.brightness, -100.0f, 100.0f, "%.1f")) {
                updateImage();
            }
            
            // Contrast slider
            if (ImGui::SliderFloat("Contrast", &params.contrast, 1.0f, 300.0f, "%.1f")) {
                updateImage();
            }
            
            // Blur size slider
            if (ImGui::SliderFloat("Blur Size", &params.blurSize, 0.0f, 15.0f, "%.1f")) {
                updateImage();
            }
            
            // Rotation slider
            if (ImGui::SliderFloat("Rotation", &params.rotationAngle, 0.0f, 360.0f, "%.1f")) {
                updateImage();
            }
            
            // // Resize slider
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
    string imagePath = "/home/quant/testing_codes_github/node_based_image_manipulation_interface/assets/anime_girl.jpg";
    
    // Check if image path is provided as command-line argument
    // if (argc > 1) {
    //     imagePath = argv[1];
    // }
    
    // Create an instance of the editor
    ImageEditorGUI editor(imagePath);
    
    // Start the application
    editor.run();
    
    return 0;
}