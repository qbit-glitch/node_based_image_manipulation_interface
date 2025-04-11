#include <opencv2/opencv.hpp>
#include <string>
#include <iostream>
#include <vector>

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
    // For non-Windows systems, use a simple terminal prompt
    string path;
    cout << "Enter the path to the image file: ";
    getline(cin, path);
    return path;
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
    // For non-Windows systems, use a simple terminal prompt
    string path;
    cout << "Enter the path to save the image file: ";
    getline(cin, path);
    return path;
#endif
}

class ImageEditorGUI {
private:
    Mat originalImage;     // Store original image for reset
    Mat workingImage;      // Current working image
    string imagePath;
    string windowName;
    string controlsWindowName;
    string buttonsWindowName;
    bool cropMode = false;
    Rect cropRect;
    Point startPoint;
    Mat tempImage;
    
    // Parameters for adjustments
    struct {
        int brightness = 0;      // Range -100 to 100
        int contrast = 100;      // Range 0 to 300 (100 is normal)
        int blurSize = 0;        // Range 0 to 15
        int rotationAngle = 0;   // Range 0 to 360
        int resizeRatio = 100;   // Range 10 to 300 (percentage)
    } params;

    // Button positions and sizes
    vector<Rect> buttonRects;
    vector<string> buttonLabels = {
        "Open Image", "Save Image", "Reset", "Grayscale", 
        "Sharpen", "Invert", "Edge Detection", "Blur", 
        "Crop Mode", "Apply Crop", "Cancel Crop"
    };

public:
    ImageEditorGUI(const string& path = "") {
        windowName = "Image Editor";
        controlsWindowName = "Adjustment Controls";
        buttonsWindowName = "Image Operations";
        imagePath = path;
        
        // Create windows
        namedWindow(windowName, WINDOW_NORMAL);
        namedWindow(controlsWindowName, WINDOW_NORMAL);
        namedWindow(buttonsWindowName, WINDOW_NORMAL);
        
        // Initialize blank working image (1x1) to prevent imshow errors
        workingImage = Mat(1, 1, CV_8UC3, Scalar(0, 0, 0));
        
        // Create buttons interface
        createButtons();
        
        // Create adjustment controls
        createTrackbars();
        
        // If path is provided, load the image
        if (!path.empty()) {
            loadImage(path);
        }
        
        // Set mouse callback for the main window (for crop functionality)
        setMouseCallback(windowName, onMouse, this);
    }
    
    ~ImageEditorGUI() {
        destroyAllWindows();
    }
    
    void loadImage(const string& path) {
        imagePath = path;
        originalImage = imread(path, IMREAD_COLOR);
        
        if (originalImage.empty()) {
            cerr << "Error: Could not open or find the image: " << path << endl;
            return;
        }
        
        workingImage = originalImage.clone();
        
        // Resize window to fit the image
        resizeWindow(windowName, min(1200, workingImage.cols), min(800, workingImage.rows));
        
        // Show the image
        imshow(windowName, workingImage);
        waitKey(1);
    }
    
    void createButtons() {
        // Create a black canvas for buttons
        Mat buttonsPanel = Mat::zeros(60, 800, CV_8UC3);
        
        // Define button rectangles
        int btnWidth = 120;
        int btnHeight = 40;
        int margin = 10;
        int x = margin;
        int y = margin;
        
        buttonRects.clear();
        
        // Draw buttons
        for (size_t i = 0; i < buttonLabels.size(); ++i) {
            if (x + btnWidth > buttonsPanel.cols) {
                x = margin;
                y += btnHeight + margin;
            }
            
            Rect btnRect(x, y, btnWidth, btnHeight);
            buttonRects.push_back(btnRect);
            
            // Draw button
            rectangle(buttonsPanel, btnRect, Scalar(120, 120, 120), -1);
            rectangle(buttonsPanel, btnRect, Scalar(200, 200, 200), 1);
            
            // Center text
            Size textSize = getTextSize(buttonLabels[i], FONT_HERSHEY_SIMPLEX, 0.5, 1, 0);
            Point textOrg(x + (btnWidth - textSize.width) / 2, 
                         y + (btnHeight + textSize.height) / 2);
            
            putText(buttonsPanel, buttonLabels[i], textOrg, 
                    FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 255), 1);
            
            x += btnWidth + margin;
        }
        
        // Show buttons
        imshow(buttonsWindowName, buttonsPanel);
        
        // Set mouse callback for the buttons window
        setMouseCallback(buttonsWindowName, onButtonClick, this);
    }
    
    void createTrackbars() {
        // Fix warning by using NULL for value pointer and properly preparing dummy variables
        createTrackbar("Brightness", controlsWindowName, NULL, 200, onTrackbarCallback, this);
        setTrackbarMin("Brightness", controlsWindowName, -100);
        setTrackbarPos("Brightness", controlsWindowName, 0);
        
        createTrackbar("Contrast", controlsWindowName, NULL, 300, onTrackbarCallback, this);
        setTrackbarMin("Contrast", controlsWindowName, 1);
        setTrackbarPos("Contrast", controlsWindowName, 100);
        
        createTrackbar("Blur Size", controlsWindowName, NULL, 15, onTrackbarCallback, this);
        
        createTrackbar("Rotation", controlsWindowName, NULL, 360, onTrackbarCallback, this);
        
        createTrackbar("Resize %", controlsWindowName, NULL, 300, onTrackbarCallback, this);
        setTrackbarMin("Resize %", controlsWindowName, 10);
        setTrackbarPos("Resize %", controlsWindowName, 100);
    }
    
    // Fixed trackbar callback
    static void onTrackbarCallback(int value, void* userdata) {
        ImageEditorGUI* instance = static_cast<ImageEditorGUI*>(userdata);
        if (instance) {
            // Read current positions from trackbars
            instance->params.brightness = getTrackbarPos("Brightness", instance->controlsWindowName);
            instance->params.contrast = getTrackbarPos("Contrast", instance->controlsWindowName);
            instance->params.blurSize = getTrackbarPos("Blur Size", instance->controlsWindowName);
            instance->params.rotationAngle = getTrackbarPos("Rotation", instance->controlsWindowName);
            instance->params.resizeRatio = getTrackbarPos("Resize %", instance->controlsWindowName);
            
            // Update the image
            instance->updateImage();
        }
    }
    
    // Button click callback
    static void onButtonClick(int event, int x, int y, int flags, void* userdata) {
        ImageEditorGUI* instance = static_cast<ImageEditorGUI*>(userdata);
        if (instance && event == EVENT_LBUTTONDOWN) {
            // Check which button was clicked
            for (size_t i = 0; i < instance->buttonRects.size(); ++i) {
                if (instance->buttonRects[i].contains(Point(x, y))) {
                    instance->handleButtonClick(i);
                    break;
                }
            }
        }
    }
    
    // Mouse callback for crop functionality
    static void onMouse(int event, int x, int y, int flags, void* userdata) {
        ImageEditorGUI* instance = static_cast<ImageEditorGUI*>(userdata);
        if (instance && instance->cropMode && !instance->workingImage.empty()) {
            if (event == EVENT_LBUTTONDOWN) {
                // Start selection
                instance->startPoint = Point(x, y);
                instance->cropRect = Rect(x, y, 0, 0);
            } else if (event == EVENT_MOUSEMOVE && (flags & EVENT_FLAG_LBUTTON)) {
                // Update selection rectangle
                instance->cropRect.width = x - instance->startPoint.x;
                instance->cropRect.height = y - instance->startPoint.y;
                
                // Clone original working image
                instance->tempImage = instance->workingImage.clone();
                
                // Draw rectangle on temp image
                rectangle(instance->tempImage, instance->cropRect, Scalar(0, 255, 0), 2);
                
                // Show the image with rectangle
                imshow(instance->windowName, instance->tempImage);
            }
        }
    }
    
    void handleButtonClick(int buttonIndex) {
        if (buttonLabels[buttonIndex] == "Open Image") {
            openImageDialog();
        } else if (buttonLabels[buttonIndex] == "Save Image") {
            saveImageDialog();
        } else if (buttonLabels[buttonIndex] == "Reset") {
            resetImage();
        } else if (buttonLabels[buttonIndex] == "Grayscale") {
            applyGrayscale();
        } else if (buttonLabels[buttonIndex] == "Sharpen") {
            applySharpen();
        } else if (buttonLabels[buttonIndex] == "Invert") {
            applyInvert();
        } else if (buttonLabels[buttonIndex] == "Edge Detection") {
            applyEdgeDetection();
        } else if (buttonLabels[buttonIndex] == "Blur") {
            applyBlur();
        } else if (buttonLabels[buttonIndex] == "Crop Mode") {
            enterCropMode();
        } else if (buttonLabels[buttonIndex] == "Apply Crop") {
            applyCrop();
        } else if (buttonLabels[buttonIndex] == "Cancel Crop") {
            cancelCrop();
        }
    }
    
    // Apply all current transformations to the image
    void applyTransformations(Mat& image) {
        if (image.empty()) return;  // Skip if image is empty
        
        // Apply resize if not 100%
        if (params.resizeRatio != 100) {
            double ratio = params.resizeRatio / 100.0;
            int newWidth = static_cast<int>(originalImage.cols * ratio);
            int newHeight = static_cast<int>(originalImage.rows * ratio);
            if (newWidth > 0 && newHeight > 0) {  // Ensure valid dimensions
                resize(image, image, Size(newWidth, newHeight), 0, 0, INTER_LINEAR);
            }
        }
        
        // Apply rotation if not 0
        if (params.rotationAngle != 0) {
            Point2f center(image.cols / 2.0f, image.rows / 2.0f);
            Mat rotMat = getRotationMatrix2D(center, params.rotationAngle, 1.0);
            warpAffine(image, image, rotMat, image.size());
        }
        
        // Apply brightness and contrast
        double alpha = params.contrast / 100.0;
        int beta = params.brightness;
        image.convertTo(image, -1, alpha, beta);
        
        // Apply blur if greater than 0
        if (params.blurSize > 0) {
            int blurSize = params.blurSize * 2 + 1;
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
        
        // Show the image
        imshow(windowName, workingImage);
        waitKey(1);
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
        params.brightness = 0;
        params.contrast = 100;
        params.blurSize = 0;
        params.rotationAngle = 0;
        params.resizeRatio = 100;
        
        // Update trackbars
        setTrackbarPos("Brightness", controlsWindowName, params.brightness);
        setTrackbarPos("Contrast", controlsWindowName, params.contrast);
        setTrackbarPos("Blur Size", controlsWindowName, params.blurSize);
        setTrackbarPos("Rotation", controlsWindowName, params.rotationAngle);
        setTrackbarPos("Resize %", controlsWindowName, params.resizeRatio);
        
        // Exit crop mode if active
        cropMode = false;
        
        // Show the image
        imshow(windowName, workingImage);
    }
    
    void applyGrayscale() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        Mat gray;
        cvtColor(workingImage, gray, COLOR_BGR2GRAY);
        cvtColor(gray, workingImage, COLOR_GRAY2BGR);  // Convert back to 3 channels
        
        // Update display
        imshow(windowName, workingImage);
    }
    
    void applySharpen() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        Mat sharpeningKernel = (Mat_<float>(3, 3) << -1, -1, -1, -1, 9, -1, -1, -1, -1);
        filter2D(workingImage, workingImage, workingImage.depth(), sharpeningKernel);
        
        // Update display
        imshow(windowName, workingImage);
    }
    
    void applyInvert() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        bitwise_not(workingImage, workingImage);
        
        // Update display
        imshow(windowName, workingImage);
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
        
        // Update display
        imshow(windowName, workingImage);
    }
    
    void applyBlur() {
        if (workingImage.empty()) {
            cout << "No image loaded yet." << endl;
            return;
        }
        
        // Apply additional blur (beyond what's in trackbar)
        GaussianBlur(workingImage, workingImage, Size(15, 15), 0);
        
        // Update display
        imshow(windowName, workingImage);
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
            
            // Update display
            imshow(windowName, workingImage);
            cout << "Image cropped successfully." << endl;
        } else {
            cout << "Invalid crop region. Please try again." << endl;
        }
    }
    
    void cancelCrop() {
        if (cropMode) {
            cropMode = false;
            // Restore the original view
            imshow(windowName, workingImage);
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
    
    // Main run loop
    void run() {
        // If no image is loaded yet, open dialog
        if (originalImage.empty()) {
            openImageDialog();
        }
        
        // Main loop
        while (true) {
            int key = waitKey(100);
            if (key == 27) { // ESC key
                break;
            }
        }
    }
};

// Main function
int main(int argc, char* argv[]) {
    string imagePath = "/Users/qbit-glitch/Desktop/Game_Developments/ImGUI Projects/image_opencv/sample_demo/assets/image.jpeg";
    
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