#include <GL/freeglut.h>
#include <opencv4/opencv2/opencv.hpp>
#include <vector>
#include <ctime>

#include <iostream>

using namespace std;
using namespace cv;

/*
 * Wartość 1 = podział ekranu na pół dla dwóch kamer
 * Wartość 2 = obraz z dwóch kamer w dwóch ćwiartkach dla dwóch kamer
 * Inne wartości zwężają dwie ćwiartki dla dwóch kamer, dla większej ilości kamer parametr ten nie ma znaczenia
 * */
#define FOUR_FRAMES_GRID_FOR_TWO_CAMERAS 2

// Ilość klatek na sekundę
#define  FPS 60

#define RED 255
#define GREEN 0
#define BLUE 0

#define TEXT_THICKNESS 1
#define BOX_AND_POINTS_THICKNESS 2

// Minimalna wartość dla jednej kamery
unsigned long cols = 1,
        rows = 1;

int frameWidth,
        frameHeight;

auto textColor = Scalar(BLUE, GREEN, RED);


TickMeter tick_meter;

// Identyfikatory tekstur OpenGL
vector<GLuint> textures;

// Ścieżki do kamer
vector<int> camerasPaths = {0}; //, 2};

// Kamery
vector<cv::VideoCapture> cameras;

// Klatki kamer
vector<cv::Mat> frames;

String fd_modelPath
, fr_modelPath;

float scoreThreshold,
        nmsThreshold;

int topK;

double cosine_similar_thresh = 0.363;
double l2norm_similar_thresh = 1.128;

Ptr<FaceDetectorYN> detector;


static void visualize(Mat &input, Mat &faces, double fps, Scalar color = textColor, int text_thickness = TEXT_THICKNESS,
                      int thickness = BOX_AND_POINTS_THICKNESS) {

    auto t = time(nullptr);
    auto now = localtime(&t);
    char result[80];

    strftime(result, sizeof(result), "%Y-%m-%d.%X", now);

    string fpsString = format("FPS: %.2f %s", fps, result);

    putText(input, fpsString, Point(0, 15), FONT_HERSHEY_SIMPLEX, 0.5, color, text_thickness);

    for (int i = 0; i < faces.rows; i++) {

        // Draw bounding box
        rectangle(input, Rect2i(int(faces.at<float>(i, 0)), int(faces.at<float>(i, 1)), int(faces.at<float>(i, 2)),
                                int(faces.at<float>(i, 3))), Scalar(0, 255, 0), thickness);
        // Draw landmarks
        circle(input, Point2i(int(faces.at<float>(i, 4)), int(faces.at<float>(i, 5))), 2, Scalar(255, 0, 0), thickness);
        circle(input, Point2i(int(faces.at<float>(i, 6)), int(faces.at<float>(i, 7))), 2, Scalar(0, 0, 255), thickness);
        circle(input, Point2i(int(faces.at<float>(i, 8)), int(faces.at<float>(i, 9))), 2, Scalar(0, 255, 0), thickness);
        circle(input, Point2i(int(faces.at<float>(i, 10)), int(faces.at<float>(i, 11))), 2, Scalar(255, 0, 255),
               thickness);
        circle(input, Point2i(int(faces.at<float>(i, 12)), int(faces.at<float>(i, 13))), 2, Scalar(0, 255, 255),
               thickness);
    }
}

void countColsAndRowsForGrid() {

    unsigned long count = camerasPaths.size();

    if (count == 1) {

        cols = rows = count;

    } else if (count % 2 == 1) {

        cols = (count - 1) / 2;
        rows = cols + 1;

    } else if (count % 2 == 0) {

        if (count > 2) {

            cols = rows = count / 2;

        } else {

            cols = count;
            rows = 1 * FOUR_FRAMES_GRID_FOR_TWO_CAMERAS;
        }
    } else {

        cols = rows = 1;
    }
}


void generateTexturesAndFrames(unsigned long);

void displayCamerasFrames(unsigned long rows, unsigned long cols);

// Funkcja wywoływana przy zmianie rozmiaru okna
void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
}

// Funkcja wywoływana przy rysowaniu zawartości okna
void display() {

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);

    displayCamerasFrames(rows, cols);

    glDisable(GL_TEXTURE_2D);

    glutSwapBuffers();
    glutPostRedisplay();
}

// Funkcja inicjalizująca OpenGL
void initGL() {

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    generateTexturesAndFrames(rows * cols);
}

// Funkcja odczytująca klatki z kamery argument int konieczny dla funkcji glutTimerFunc
void captureFrames(int) {

    int k = 0;
    Mat faces,
            frame;

    for (auto i: cameras) {

        frameWidth = int(i.get(CAP_PROP_FRAME_WIDTH));
        frameHeight = int(i.get(CAP_PROP_FRAME_HEIGHT));

        cout<<"Frame WIDTH: "<<frameWidth<<" Frame HEIGHT: "<<frameHeight<<endl;

        detector->setInputSize(Size(frameWidth, frameHeight));

        tick_meter.start();

        i.read(frame);

        detector->detect(frame, faces);

        tick_meter.stop();

        visualize(frame, faces, tick_meter.getFPS());

        frames[k] = frame;

        k++;
    }

    glutTimerFunc(1, captureFrames, 0);
    glutPostRedisplay();

}

GLuint initTexture() {

    GLuint texture;

    // Inicjalizacja tekstur OpenGL
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    /* Mieszanie kanałami RGBA tekstury, dodaje efekty na kolorach,
     * zmieniamy literkę na końcu flagi, możliwe kanały R G B A oraz razem RGBA,
     * kolejna flaga robi efekt na wybranyn kanale,
     * możliwe flagi GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ZERO and GL_ONE
     * glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_R, GL_ONE); */

    return texture;
}

void generateTexturesAndFrames(unsigned long number) {

    for (int i = 0; i < number; i++) {

        textures.emplace_back(initTexture());

        frames.emplace_back();
    }
}

void displayCamerasFrames(unsigned long rows, unsigned long cols) {


    CV_Assert(!frames.empty());

    unsigned int maxTextures = textures.size();
    unsigned int maxFrames = frames.size();

    CV_Assert(!(maxFrames != maxTextures));


    float x = (glutGet(GLUT_SCREEN_WIDTH) / cols), y = (glutGet(GLUT_SCREEN_HEIGHT) / rows);

    unsigned long t = 0;

    for (unsigned long k = 0; k < rows; k++) {

        for (unsigned long j = 0; j < cols; j++) {

            if (maxTextures <= t) break;


            // Wyświetlanie klatki z kamery
            glBindTexture(GL_TEXTURE_2D, textures[t]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frames[t].cols, frames[t].rows, 0, GL_BGR, GL_UNSIGNED_BYTE,
                         frames[t].ptr());

            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f((x * j), (y * k));
            //cout << "p00 = " << (x * j) << ":" << (y * k) << endl;
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f((x + (x * j)), (y * k));
            //cout << "p10 = " << (x + (x * j)) << ":" << (y * k) << endl;
            glTexCoord2f(1.0f, 1.0f);
            glVertex2f((x + (x * j)), (y + (y * k)));
            //cout << "p11 = " << (x + (x * j)) << ":" << (y + (y * k)) << endl;
            glTexCoord2f(0.0f, 1.0f);
            glVertex2f((x * j), (y + (y * k)));
            //cout << "p01 = " << (x * j) << ":" << (y + (y * k)) << endl;
            glEnd();

            t++;

        }

        if (maxTextures <= t) break;
    }

}

void initCameras() {

//    frameWidth = glutGet(GLUT_SCREEN_WIDTH) / cols;
//    frameHeight = glutGet(GLUT_SCREEN_HEIGHT) / rows;


    int k = 0;

    for (const auto &i: camerasPaths) {

        cameras.emplace_back(i);

        auto cam = cameras[k];

        CV_Assert(cam.isOpened());

        // Ustawienie rozmiarów klatek
//        cam.set(cv::CAP_PROP_FRAME_WIDTH, frameWidth);
//        cam.set(cv::CAP_PROP_FRAME_HEIGHT, frameHeight);
        cam.set(CAP_PROP_FPS, FPS);

//        cout<<"Frame Width: "<<cam.get(CAP_PROP_FRAME_WIDTH)<<" Frame Height: "<<cam.get(CAP_PROP_FRAME_HEIGHT)<<endl;


        k++;
    }

}

int main(int argc, char **argv) {

    CommandLineParser parser(argc, argv,
                             "{help  h      |            | Print this message}"
                             "{fd_model fd       | face_detection_yunet_2021dec.onnx| Path to the model. Download yunet.onnx in https://github.com/opencv/opencv_zoo/tree/master/models/face_detection_yunet}"
                             "{fr_model fr       | face_recognition_sface_2021dec.onnx | Path to the face recognition model. Download the model at https://github.com/opencv/opencv_zoo/tree/master/models/face_recognition_sface}"
                             "{score_threshold   | 0.9        | Filter out faces of score < score_threshold}"
                             "{nms_threshold     | 0.3        | Suppress bounding boxes of iou >= nms_threshold}"
                             "{top_k             | 5000       | Keep top_k bounding boxes before NMS}"
    );


    if (parser.has("help")) {
        parser.printMessage();
        return 0;
    }

    fd_modelPath = parser.get<String>("fd_model");
    fr_modelPath = parser.get<String>("fr_model");

    scoreThreshold = parser.get<float>("score_threshold");
    nmsThreshold = parser.get<float>("nms_threshold");
    topK = parser.get<int>("top_k");


    // Initialize FaceDetectorYN
    detector = FaceDetectorYN::create(fd_modelPath, "", Size(320, 320), scoreThreshold, nmsThreshold, topK);


    countColsAndRowsForGrid();

    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(glutGet(GLUT_SCREEN_WIDTH), glutGet(GLUT_SCREEN_HEIGHT));
    glutCreateWindow("Monitoring");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);

    initGL();

    initCameras();

    glutTimerFunc(0, captureFrames, 0);

    glutMainLoop();

    return 0;
}
