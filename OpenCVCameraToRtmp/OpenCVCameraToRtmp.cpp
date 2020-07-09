﻿// OpenCVCameraToRtmp.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>


using namespace cv;
using namespace std;

#pragma comment(lib,"opencv_world440")

int main()
{
	VideoCapture cap;
	int ret = cap.open(0, CAP_DSHOW);

	while (1)
	{
		Mat frame;//定义一个变量把视频源一帧一帧显示
		cap >> frame;

		if (frame.empty())
		{
			cout << "Finish" << endl;
			break;
		}
		imshow("Input video", frame);
		waitKey(30);
	}
	cap.release();

}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
