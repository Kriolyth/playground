#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

enum CursorType { cursor_Normal, cursor_Busy, cursor_DragHorz, cursor_DragVert, cursor_Create };

void googleSearch( cv::Mat& img );
void imgFromClipboard( cv::Mat& img );
void setCursor( CursorType cursorType );