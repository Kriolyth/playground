// autoCut.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;
using namespace std; 

enum LineStyles { lnEdge };

Mat src, src_gray, src_hue;
Mat dst;
Mat edges_gray, edges_hue;
Mat hough;

int edgeThresh = 1;
int lowThreshold;
int const max_lowThreshold = 100;
int ratio[2] = { 5, 2 };  // Ratio = multipler / divisor
int kernel_size = 3;
char* window_canny = "Edge Map";
char* window_hough = "Detected Lines";

// Lines found
vector<Vec4i> lines;


void print_help()
{
	cout << " Usage: autoCut <image-filename>" << endl;
}
void draw_line( Mat& image, Point start, Point end, int style = lnEdge )
{
	switch( style ) {
		case lnEdge: cv::line( image, start, end, Scalar( 0, 0, 255 ), 2, CV_AA ); break; 
	}
}


void performCanny()
{
	// a vertical blur
	Size blur_kernel( 3, 3 ); 

	// Reduce noise and detect for light channel
	blur( src_gray, edges_gray, blur_kernel );

	Canny( edges_gray, edges_gray, 
		lowThreshold, lowThreshold * ratio[0] / ratio[1], 
		kernel_size );

	// Reduce noise and detect for hue channel
	blur( src_hue, edges_hue, blur_kernel );

	Canny( edges_hue, edges_hue, 
		lowThreshold, lowThreshold * ratio[0] / ratio[1], 
		kernel_size );

	// Using Canny's output as a mask, we display our result
	dst = Scalar::all(0);

	//cv::addWeighted( edges_hue, 0.5, edges_gray, 0.5, 1.0, edges_gray );
	cv::add( edges_hue, edges_gray, edges_gray );

}

template<typename T> struct AxisAlignedPred {
	typedef T argument_type;
	bool operator()( const T& item ) const {
		return (item[0] == item[2]) || (item[1] == item[3]);
	}
};

void performHough()
{
	const int dilation_size = 1;
	const int distResolution = 2;
	const double angleResolution = CV_PI / 32;
	const int minLineLength = 80;

	//cvtColor( edges_gray, hough, CV_GRAY2BGR );
	Mat element = getStructuringElement( MORPH_CROSS,
		Size( 2*dilation_size + 1, 2*dilation_size+1 ),
		Point( dilation_size, dilation_size ) );

	// Apply the erosion operation
	dilate( edges_gray, hough, element, Point(-1,-1), 2 );	
	//edges_gray.assignTo( hough );

	lines.clear();
	HoughLinesP( hough, lines, distResolution, angleResolution, minLineLength, minLineLength, 10 );

	// Leave only axially-aligned lines
	//std::remove_if( lines.begin(), lines.end(), ( AxisAlignedPred<Vec4i>() ) );	
}

// Redisplay image
void redraw()
{
	// Draw Canny window
	//src.copyTo( dst, edges_gray);
	cvtColor( edges_gray, dst, CV_GRAY2BGR );
	imshow( window_canny, dst );

	src.copyTo( dst );
	for( size_t i = 0; i < lines.size(); i++ )
	{
		Vec4i l = lines[ i ];
		if ( AxisAlignedPred<Vec4i>()( l ) )
			draw_line( dst, Point( l[0], l[1] ), Point( l[2], l[3] ) );
	}

	imshow( window_hough, dst );
}

// Callback, performing Canny detector
void trackbarCallback(int, void*)
{
	performCanny();
	performHough();
	redraw();
}

// Extract a single channel (zero-based)
void ExtractChannel( Mat& src, Mat& dest, unsigned channel )
{
	vector<Mat> chans(3);	
	split( src, chans );
	assert( channel < chans.size() );
	chans[channel].assignTo( dest );
}

int _tmain(int argc, _TCHAR* argv[])
{
	if( argc != 2 )
	{
		print_help();
		return 1;
	}

	src = imread( fromUtf16( argv[1] ).c_str(), cv::IMREAD_COLOR );
	if ( !src.data )
	{
		cerr << "Cannot read image" << endl;
		return 2;
	}

	//draw_line( image, Point( 20, 0 ), Point( 20, 60 ) );
	// Create a matrix of the same type and size as src (for dst)
	dst.create( src.size(), src.type() );

	// I'd like to track color and luminosity changes separately
	{
		Mat src_hsv;
		cvtColor( src, src_hsv, CV_BGR2HSV );
		ExtractChannel( src_hsv, src_hue, 0 );
		ExtractChannel( src_hsv, src_gray, 2 );
	}

	// Convert the image to grayscale
	//cvtColor( src, src_gray, CV_BGR2GRAY );

	// Create a window
	namedWindow( window_canny, CV_WINDOW_AUTOSIZE );
	namedWindow( window_hough, CV_WINDOW_AUTOSIZE );

	// Create a Trackbar for user to enter threshold
	createTrackbar( "Min Threshold:", window_canny, &lowThreshold, max_lowThreshold, trackbarCallback );

	// Detect edges and redisplay image
	trackbarCallback(0, 0);
	//namedWindow( "Source", CV_WINDOW_AUTOSIZE );
	//imshow( "Source", src_hue );

	waitKey( 0 );
	
	return 0;
}

