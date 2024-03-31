#include <GL/freeglut.h>
#include <opencv4/opencv2/opencv.hpp>
#include <vector>
#include <ctime>

#include <iostream>

using namespace std;
using namespace cv;

// Wartość 1 = podział ekranu na pół dla dwóch kamer
// Wartość 2 = obraz z dwóch kamer w dwóch ćwiartkach dla dwóch kamer
// Inne wartości zwężają dwie ćwiartki dla dwóch kamer, dla większej ilości kamer parametr ten nie ma znaczenia
#define FOUR_FRAMES_GRID_FOR_TWO_CAMERAS 2

// Ilość klatek na sekundę
#define  FPS 60
// Minimalna wartość dla jednej kamery
unsigned long cols = 1,
        rows = 1;

int frameWidth,
        frameHeight;

TickMeter tickmeter;

// Identyfikatory tekstur OpenGL
vector<GLuint> textures;

// Ścieżki do kamer
vector<int> camerasPaths = {0, 2};

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

// Dostępne kamery
vector<cv::VideoCapture> cameras;

// Klatki dla kamer
vector<cv::Mat> frames;


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

    for (auto i: cameras) {

        tickmeter.start();

        i.read(frames[k]);

        tickmeter.stop();

        auto t = time(0);
        auto now = localtime(&t);
        char result[80];
        strftime(result, sizeof(result), "%Y-%m-%d.%X", now);

        string fpsString = format("FPS: %.2f %s", tickmeter.getFPS(), result);
        putText(frames[k], fpsString, Point(0, 15), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255), 1.5);


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

    frameWidth = glutGet(GLUT_SCREEN_WIDTH) / cols;
    frameHeight = glutGet(GLUT_SCREEN_HEIGHT) / rows;

    int k = 0;

    for (const auto &i: camerasPaths) {

        cameras.emplace_back(i);

        auto cam = cameras[k];

        CV_Assert(cam.isOpened());


        // Ustawienie rozmiarów klatek
        cam.set(cv::CAP_PROP_FRAME_WIDTH, frameWidth);
        cam.set(cv::CAP_PROP_FRAME_HEIGHT, frameHeight);
        cam.set(CAP_PROP_FPS,FPS);

        k++;
    }

}

int main(int argc, char **argv) {

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
