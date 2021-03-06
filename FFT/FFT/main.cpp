#include <math.h>
#include <fstream>
#include <iostream>
#include "opencv2/core.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/highgui.hpp"

const float PI = 3.14159265f;

void dft(short** in_array, double** re_array, double** im_array, float height, float width)
{
    double re, im, temp;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            re = 0;
            im = 0;

            for (int x = 0; x < height; x++) {
                for (int y = 0; y < width; y++) {
                    temp = (double)i * x / (double)height +
                        (double)j * y / (double)width;
                    re += in_array[x][y] * cos(-2.0 * PI * temp);
                    im += in_array[x][y] * sin(-2.0 * PI * temp);
                }
            }

            re_array[i][j] = re;
            im_array[i][j] = im;
        }
    }
}

void fre_spectrum(char** in_array, char** out_array, float height, float width)
{
    double re, im, temp;
    int move;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            re = 0;
            im = 0;

            for (int x = 0; x < height; x++) {
                for (int y = 0; y < width; y++) {
                    temp = (double)i * x / (double)height +
                        (double)j * y / (double)width;
                    move = (x + y) % 2 == 0 ? 1 : -1;
                    re += in_array[x][y] * cos(-2.0 * PI * temp) * move;
                    im += in_array[x][y] * sin(-2.0 * PI * temp) * move;
                }
            }

            out_array[i][j] = (char)(sqrt(re * re + im * im) / sqrt(double(width) * height));
            if (out_array[i][j] > 0xff)
                out_array[i][j] = 0xff;
            else if (out_array[i][j] < 0)
                out_array[i][j] = 0;
        }
    }
}

int main()
{
    cv::Mat image = cv::imread("D:\Document\Vulkan\FFT\FFT\1.png");
    std::cout << image.size << std::endl;
    return 0;
}