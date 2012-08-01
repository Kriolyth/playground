// autoCut.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;
using namespace std; 

enum LineStyles { lnEdge };

Mat src, src_lab, src_gray, src_diff;
Mat dst, dst_canny, dst_hough;
Mat edges_gray;
Mat hough;

int edgeThresh = 1;
int lowThreshold = 2;
int const max_lowThreshold = 20;
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
		case lnEdge: cv::line( image, start, end, Scalar( 0, 128, 255 ), 2, CV_AA ); break; 
	}
}


template<typename T> struct AxisAlignedPred {
	typedef T argument_type;
	bool operator()( const T& item ) const {
		return (item[0] == item[2]) || (item[1] == item[3]);
	}
};

template<typename T> void performAccum( Mat& img )
{
	int imgWidth = img.cols;
	vector<float> accum( imgWidth ), momentum( imgWidth ), prevLine( imgWidth );
	MatIterator_<T> it, end;
	vector<float>::iterator acIt, acEnd, mmIt, mmEnd, lnIt, lnEnd;

	accum.resize( imgWidth );
	for( it = img.begin<T>(), end = img.end<T>(); it != end; )
	{
		for ( acIt = accum.begin(), acEnd = accum.end(), 
				mmIt = momentum.begin(), mmEnd = momentum.end(),
				lnIt = prevLine.begin(), lnEnd = prevLine.end();
				acIt != acEnd && mmIt != mmEnd && lnIt != lnEnd && it != end;
				++acIt, ++mmIt, ++lnIt, ++it )
		{
			*acIt += *it;
			if ( *it > 0.01 ) {
				*acIt += *mmIt;
				*mmIt += *it;
				*mmIt *= (float)1.025;  // stability bonus
			} else {
				if ( *mmIt > 1 ) *mmIt /= 2;
				else *mmIt = 0;
			}
		}
	}

	// Sort found lines according to accumulator
	using namespace boost;
	struct Rank {
		int pos;
		float value;
		bool operator >( const Rank& rhs ) const { return value > rhs.value; }
		bool inRange( const Rank& rhs ) const { 
			int p1 = pos;
			int p2 = rhs.pos;
			return fast_abs( p1 - p2 ) < 5; 
		}
	};
	vector<Rank> rank;
	// omit border lines
	for ( int i = 10; i < img.cols-10; ++i )
	{
		Rank r = { i, accum[i] };
		rank.push_back( r );
	}
	std::sort( rank.begin(), rank.end(), std::greater<Rank>() );

	// Store all found lines in order of appearance
	for ( size_t i = 0; i < rank.size(); ++i )
	{
		// skip neighbouring lines
		if ( i != 0 ) {
			vector<Rank>::iterator rank_end = rank.begin() + i;
			if ( std::find_if( rank.begin(), rank_end, 
				bind( &Rank::inRange, &rank[i], _1 ) ) != rank_end )
				continue;
		}

		lines.push_back( Vec4i( rank[i].pos, 0, rank[i].pos, img.rows ) );
	}

}

// Redisplay image
void redraw()
{
	src.copyTo( dst_hough );
	for ( int i = 0; i < std::min<int>( lowThreshold, (int)lines.size() ); ++i )
	{
		Vec4i l = lines[ i ];
		draw_line( dst_hough, Point( l[0], l[1] ), Point( l[2], l[3] ) );
	}

	imshow( window_hough, dst_hough );
}

// Callback, performing Canny detector
void trackbarCallback(int, void*)
{

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

	

	// Now I'd like to convert to CIELab space to track "perceptive" 
	// difference between neighbouring rows or columns
	{
		Mat tmp( src.rows, src.cols, CV_32FC3 );
		cvtColor( src, tmp, CV_BGR2Lab );
		tmp.copyTo( src_lab );
	}

	// In order to grab the "perceptive" difference I need differences
	// for each channel
	// Subtract neighbouring pixels with a kernel [1 -1]
	/*Mat kernel = (Mat_<double>( 3, 3) <<  0,  0,  0,
										  0,  1, -1,
										  0,  0,  0); // */
	Mat kernel = (Mat_<double>( 2, 2) <<  1,  -1,
										  1,  -1 );   // */
	filter2D( src_lab, src_diff, CV_32FC3, kernel, Point(0,0) );  // */

	// we have per-channel differences, all it takes now is to square them and add together
	{
		Mat mul_mat;
		vector<Mat> chans(3);

		// Square + bring to range [0..1]
		multiply( src_diff, src_diff, mul_mat, 1.0 / 65536, CV_32FC3 );

		split( mul_mat, chans );
		add( chans[0], chans[1], src_gray );
		add( chans[2], src_gray, src_gray );

		src_gray /= 3;

		cv::sqrt( src_gray, src_gray );
		//src_gray *= 4;
		//multiply( src_gray, src_gray, src_gray );
	}

	// Line detecting accumulator
	performAccum<float>( src_gray );

	//src.copyTo( src_diff );
	cvtColor( src_gray, src_diff, CV_GRAY2BGR );
	multiply( src, src_diff, src_diff, 1, CV_8UC1 );
	namedWindow( "Diff", CV_WINDOW_AUTOSIZE );
	imshow( "Diff", src_diff );

	// Guess the number of lines
	lowThreshold = cvRound( (double)src.cols / src.rows - 0.25 ) - 1;

	// Create a window
	//namedWindow( window_canny, CV_WINDOW_AUTOSIZE );
	namedWindow( window_hough, CV_WINDOW_AUTOSIZE );

	// Create a Trackbar for user to enter threshold
	createTrackbar( "Min Threshold:", window_hough, &lowThreshold, max_lowThreshold, trackbarCallback );

	// Detect edges and redisplay image
	trackbarCallback(0, 0);

	waitKey( 0 );
	
	return 0;
}

