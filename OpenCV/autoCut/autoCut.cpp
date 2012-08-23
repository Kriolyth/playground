// autoCut.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "extStuff.h" 

using namespace cv;
using namespace std; 

enum LineStyles { lnEdge };

Mat src, src_lab, src_gray;
Mat dst, dst_canny, dst_hough;
Mat edges_gray;
Mat hough;

int edgeThresh = 1;
int lowThreshold = 2;
int const max_lowThreshold = 20;
char* window_hough = "Detected Lines";
char* trackbar_name = "Min Threshold:";

// Lines found
vector<Vec4i> lines;

struct MouseCapture {
	bool mouseDown;
	int lineId;
} mouseCapture;


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

	// go over the image pixels and accumulate all vertical lines
	// We give boost "bonus" for an uninterrupted line - the longer, the better.
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
				*mmIt *= (float)1.025;  // steady exponential growth
			} else {
				// if line is broken - quickly decay the bonus
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

int trackDivider( int x, int y )
{
	// Find the nearest line (not only displayed)
	Vec4i nearest(-1,-1,-1,-1);
	int counter(0);

	vector<Vec4i>::const_iterator it, itEnd;
	for ( it = lines.begin(), itEnd = lines.end();
		it != itEnd && counter < lowThreshold; ++it, ++counter )
	{
		if ( fast_abs( (*it)[0] - x ) <= 2 ) 
		{
			nearest = *it;
			return counter;
		}
	}

	return -1;
}

void lookupImage( int x, int y )
{
	// Extract a corresponding image part
	// To do this, we should find closest bounding lines
	Vec4i lowLine(0,0,0,src.rows-1), highLine(src.cols-1,0,src.cols,src.rows);
	
	vector<Vec4i>::const_iterator it, itEnd;
	for ( it = lines.begin(), itEnd = lines.begin() + lowThreshold;
		it != itEnd; ++it )
	{
		if ( (*it)[0] < x && lowLine[0] < (*it)[0] )
			lowLine = *it;
		if ( (*it)[0] > x && highLine[0] > (*it)[0] )
			highLine = *it;
	}

	// Extract the region
	Rect roi( lowLine[0], lowLine[1], highLine[2] - lowLine[0], highLine[3] );
	Mat subimg = src( roi );

	googleSearch( subimg );
}

void actOnDivider( int x, int y, int divider, bool clicked )
{
	//if ( divider >= 0 && mouseCapture.lineId == -1 )
	//	mouseCapture.lineId = counter;
	//else if ( displayed && !clicked && !mouseCapture.mouseDown )
	//	mouseCapture.lineId = -1;

	if ( !clicked ) {
		// we hovered a line, but we should only react
		// to this if it is visible
		if ( !mouseCapture.mouseDown ) {
			// we're not dragging a line
		} else {
			// we're dragging a line -- update position
			assert( mouseCapture.lineId >= 0 && mouseCapture.lineId < lines.size() );
			Vec4i vec = lines[ mouseCapture.lineId ];
			vec[0] = x; vec[2] = x;
			lines[ mouseCapture.lineId ] = vec;
			redraw();
		}
	} else {
		if ( divider < 0 ) {
			// we clicked "somewhere" - we should create a new line 
			// and add it to the lines vector
			Vec4i vec( x, 0, x, src.rows );
			lines.insert( lines.begin() + lowThreshold, vec );
			mouseCapture.lineId = lowThreshold;
			mouseCapture.mouseDown = true;
			setTrackbarPos( trackbar_name, window_hough, lowThreshold + 1 );
		} else {
			// we clicked a visible line -- slide it
			mouseCapture.lineId = divider;
			mouseCapture.mouseDown = true;
			redraw();
		}
	}

}

void mouseCallback(int event, int x, int y, int flags, void* userdata)
{
	switch( event )
	{
	case EVENT_LBUTTONDOWN:
		{
			int divider( trackDivider( x, y ) );
			if ( -1 == divider && !( flags & EVENT_FLAG_SHIFTKEY ) )
				lookupImage( x, y );
			else
				actOnDivider( x, y, divider, true );

			break;
		}
	case EVENT_MOUSEMOVE:
		{
			int divider( trackDivider( x, y ) );

			if ( divider >= 0 )
				setCursor( cursor_DragHorz );
			else if ( flags & EVENT_FLAG_SHIFTKEY )
				setCursor( cursor_Create );
			else
				setCursor( cursor_Normal );

			actOnDivider( x, y, divider, false );
			break;
		}
	case EVENT_LBUTTONUP:
		{
			mouseCapture.mouseDown = false;
			mouseCapture.lineId = -1;
			break;
		}
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	if( argc != 2 )
	{
		print_help();
		return 1;
	}

	string arg( fromUtf16( argv[1] ) );
	if ( arg == "--from-clipboard" )
	{
		cout << "Reading from clipboard" << endl;
		imgFromClipboard( src );
	}
	else
	{
		cout << "Loading file " << arg << endl;
		src = imread( arg.c_str(), cv::IMREAD_COLOR );
	}

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


	// we have per-channel differences, all it takes now is to square them and add together
	{
		Mat mul_mat, src_diff;
		vector<Mat> chans(3);

		// In order to grab the "perceptive" difference I need differences
		// for each channel
		// Subtract neighbouring pixels with a kernel [1 -1]
		/*Mat kernel = (Mat_<double>( 3, 3) <<  0,  0,  0,
											  0,  1, -1,
											  0,  0,  0); // */
		Mat kernel = (Mat_<double>( 2, 2) <<  1,  -1,
											  1,  -1 );   // */
		filter2D( src_lab, src_diff, CV_32FC3, kernel, Point(0,0) );  // */

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
	//cvtColor( src_gray, src_diff, CV_GRAY2BGR );
	//multiply( src_lab, src_diff, src_diff, 1, CV_8UC1 );
	//namedWindow( "Diff", CV_WINDOW_AUTOSIZE );
	//imshow( "Diff", src_diff );

	// Guess the number of lines
	lowThreshold = cvRound( (double)src.cols / src.rows - 0.25 ) - 1;

	// Create a window
	//namedWindow( window_canny, CV_WINDOW_AUTOSIZE );
	namedWindow( window_hough, CV_WINDOW_AUTOSIZE );


	// Create a Trackbar for user to enter threshold
	createTrackbar( trackbar_name, window_hough, &lowThreshold, max_lowThreshold, trackbarCallback );

	// Detect edges and redisplay image
	trackbarCallback(0, 0);

	setMouseCallback( window_hough, mouseCallback, NULL );

	int key;
	while ( ( key = waitKey( 0 ) ) != VK_ESCAPE )
	{
		if ( VK_DELETE == key && mouseCapture.mouseDown ) {
			if ( mouseCapture.lineId >= 0 && mouseCapture.lineId < lines.size() ) {
				lines.erase( lines.begin() + mouseCapture.lineId );
				mouseCapture.mouseDown = false;
				mouseCapture.lineId = -1;
				if ( lowThreshold > 0 ) --lowThreshold;
				setTrackbarPos( trackbar_name, window_hough, lowThreshold );
			}
		}
	}
	
	return 0;
}

int WINAPI WinMain ( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
	LPWSTR* argv;
	int argc;
	argv = CommandLineToArgvW( GetCommandLine(), &argc );
	return _tmain( argc, argv );
}