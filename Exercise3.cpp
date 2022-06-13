
#define TEST 0


#include<opencv2/opencv.hpp>
#include<iostream>



using namespace cv;
using namespace std;


#define EX5 1
#define EX5_RAW 0
#define DRAW_CONTOUR 0
#define DRAW_RECTANGLE 0

#define THICKNESS_VALUE 4

// Struct holding all infos about each strip, e.g. length
struct MyStrip {
	int stripeLength;
	int nStop;
	int nStart;
	Point2f stripeVecX;
	Point2f stripeVecY;
};

// List of points
typedef vector<Point> contour_t;
// List of contours
typedef vector<contour_t> contour_vector_t;


const int threshold_slider_max = 255;
int threshold_slider = 0;

const int fps = 30;

Mat videoStreamFrameGray;
Mat videoStreamFrameOutput;

const string stripWindow = "Strip Window";

// Pos is from UI, dereferencing of the pointer
static void on_trackbar(int pos, void* slider_value) {
	*((int*)slider_value) = pos;
	// C++ >= 11 -> Standard
	//*(static_cast<int*>(slider_value)) = pos;
}

int subpixSampleSafe(const Mat& pSrc, const Point2f& p) {
	// floorf -> like int casting, but -2.3 will be the smaller number -> -3
	// Point is float, we want to know which color does it have
	int fx = int(floorf(p.x));
	int fy = int(floorf(p.y));

	if (fx < 0 || fx >= pSrc.cols - 1 ||
		fy < 0 || fy >= pSrc.rows - 1)
		return 127;

	// Slides 15
	int px = int(256 * (p.x - floorf(p.x)));
	int py = int(256 * (p.y - floorf(p.y)));

	// Here we get the pixel of the starting point
	unsigned char* i = (unsigned char*)((pSrc.data + fy * pSrc.step) + fx);

	// Shift 2^8
	// Internsity
	int a = i[0] + ((px * (i[1] - i[0])) >> 8);
	i += pSrc.step;
	int b = i[0] + ((px * (i[1] - i[0])) >> 8);

	// We want to return Intensity for the subpixel
	return a + ((py * (b - a)) >> 8);
}


// Added in Sheet 3 - Ex7 (a) Start *****************************************************************
Mat calculate_Stripe(double dx, double dy, MyStrip& st) {
	// Norm (euclidean distance) from the direction vector is the length (derived from the Pythagoras Theorem)
	double diffLength = sqrt(dx * dx + dy * dy);

	// Length proportional to the marker size
	st.stripeLength = (int)(0.8 * diffLength);

	if (st.stripeLength < 5)
		st.stripeLength = 5;

	// Make stripeLength odd (because of the shift in nStop), Example 6: both sides of the strip must have the same length XXXOXXX
	//st.stripeLength |= 1;
	if (st.stripeLength % 2 == 0)
		st.stripeLength++;

	// E.g. stripeLength = 5 --> from -2 to 2: Shift -> half top, the other half bottom
	//st.nStop = st.stripeLength >> 1;
	st.nStop = st.stripeLength / 2;
	st.nStart = -st.nStop;

	Size stripeSize;

	// Sample a strip of width 3 pixels
	stripeSize.width = 3;
	stripeSize.height = st.stripeLength;

	// Normalized direction vector
	st.stripeVecX.x = dx / diffLength;
	st.stripeVecX.y = dy / diffLength;

	// Normalized perpendicular direction vector (rotated 90?clockwise, rotation matrix)
	st.stripeVecY.x = st.stripeVecX.y;
	st.stripeVecY.y = -st.stripeVecX.x;

	// 8 bit unsigned char with 1 channel, gray
	return Mat(stripeSize, CV_8UC1);
}

int main() {
	Mat frame;
	VideoCapture cap(1);

	const string streamWindow = "Stream";

	if (!cap.isOpened()) {
		cout << "No webcam, using video file" << endl;
		cap.open("MarkerMovie.MP4");
		if (cap.isOpened() == false) {
			cout << "No video!" << endl;
			exit(0);
			return -1;
		}
	}

	// Added in Sheet 3 - Start *****************************************************************

	bool isFirstStripe = true;

	// Added in Sheet 3 - End *******************************************************************

	const string contoursWindow = "Contours";
	const string UI = "Threshold";
	namedWindow(contoursWindow, CV_WINDOW_FREERATIO);
	//namedWindow(stripWindow, CV_WINDOW_AUTOSIZE);

	int slider_value = 100;
	createTrackbar(UI, contoursWindow, &slider_value, 255, on_trackbar, &slider_value);

	Mat imgFiltered;

	while (cap.read(frame)) {

		// --- Process Frame ---

		Mat grayScale;
		imgFiltered = frame.clone();
		cvtColor(imgFiltered, grayScale, COLOR_BGR2GRAY);

		// Threshold to reduce the noise
		threshold(grayScale, grayScale, slider_value, 255, THRESH_BINARY);

		contour_vector_t contours;

		// RETR_LIST is a list of all found contour, SIMPLE is to just save the begin and ending of each edge which belongs to the contour
		findContours(grayScale, contours, RETR_LIST, CHAIN_APPROX_SIMPLE);

		//drawContours(imgFiltered, contours, -1, Scalar(0, 255, 0), 4, 1);

		// size is always positive, so unsigned int -> size_t; if you have not initialized the vector it is -1, hence crash
		for (size_t k = 0; k < contours.size(); k++) {

			// -------------------------------------------------

			// --- Process Contour ---

			contour_t approx_contour;

			// Simplifying of the contour with the Ramer-Douglas-Peuker Algorithm
			// true -> Only closed contours
			// Approximation of old curve, the difference (epsilon) should not be bigger than: perimeter(->arcLength)*0.02
			approxPolyDP(contours[k], approx_contour, arcLength(contours[k], true) * 0.02, true);

#if DRAW_CONTOUR
			contour_vector_t cov, aprox;
			cov.emplace_back(contours[k]);
			aprox.emplace_back(approx_contour);
			if (approx_contour.size() > 1) {
				drawContours(imgFiltered, aprox, -1, Scalar(0, 255, 0), 4, 1);
				drawContours(imgFiltered, cov, -1, Scalar(255, 0, 0), 4, 1);
				continue;
			}
#endif // DRAW_CONTOUR

			Scalar QUADRILATERAL_COLOR(0, 0, 255);
			Scalar colour;
			// Convert to a usable rectangle
			Rect r = boundingRect(approx_contour);
 
#if DRAW_RECTANGLE
			rectangle(imgFiltered, r, Scalar(0, 0, 255), 4);
			continue;
#endif //DRAW_RECTANGLE

			// 4 Corners -> We color them
			if (approx_contour.size() == 4) {
				colour = QUADRILATERAL_COLOR;
			}
			else {
				continue;
			}

			// --- Filter tiny ones --- If the found contour is too small (20 -> pixels, frame.cols - 10 to prevent extreme big contours)
			if (r.height < 20 || r.width < 20 || r.width > imgFiltered.cols - 10 || r.height > imgFiltered.rows - 10) {
				continue;
			}

			// -> Cleaning done

			// 1 -> 1 contour, we have a closed contour, true -> closed, 4 -> thickness
			polylines(imgFiltered, approx_contour, true, colour, THICKNESS_VALUE);

			// -----------------------------

			// --- Process Corners ---

			for (size_t i = 0; i < approx_contour.size(); ++i) {
				// Render the corners, 3 -> Radius, -1 filled circle
				circle(imgFiltered, approx_contour[i], 3, CV_RGB(0, 255, 0), -1);

				// Euclidic distance, 7 -> parts, both directions dx and dy
				double dx = ((double)approx_contour[(i + 1) % 4].x - (double)approx_contour[i].x) / 7.0;
				double dy = ((double)approx_contour[(i + 1) % 4].y - (double)approx_contour[i].y) / 7.0;

				// Added in Sheet 3 - Start *****************************************************************

				MyStrip strip;

				// A simple array of unsigned char cv::Mat
				Mat imagePixelStripe = calculate_Stripe(dx, dy, strip);

				// Added in Sheet 3 - End *******************************************************************

				// First point already rendered, now the other 6 points
				for (int j = 1; j < 7; ++j) {
					// Position calculation
					double px = (double)approx_contour[i].x + (double)j * dx;
					double py = (double)approx_contour[i].y + (double)j * dy;

					Point p;
					p.x = (int)px;
					p.y = (int)py;
					circle(imgFiltered, p, 2, CV_RGB(0, 0, 255), -1);

//------------------------------------------- EX 3 ---------------------------------------------------------

					// Columns: Loop over 3 pixels
					for (int m = -1; m <= 1; ++m) { // -1 0 1
						// Rows: From bottom to top of the stripe, e.g. -3 to 3
						for (int n = strip.nStart; n <= strip.nStop; ++n) {
							Point2f subPixel;

							// m -> going over the 3 pixel thickness of the stripe, n -> over the length of the stripe, direction comes from the orthogonal vector in st
							// Going from bottom to top and defining the pixel coordinate for each pixel belonging to the stripe
							subPixel.x = (double)p.x + ((double)m * strip.stripeVecX.x) + ((double)n * strip.stripeVecY.x);
							subPixel.y = (double)p.y + ((double)m * strip.stripeVecX.y) + ((double)n * strip.stripeVecY.y);

							// Just for markings in the image!
							Point p2;
							p2.x = (int)subPixel.x;
							p2.y = (int)subPixel.y;

							// The one (purple color) which is shown in the stripe window
							if (isFirstStripe)
								circle(imgFiltered, p2, 1, CV_RGB(255, 0, 255), -1);
							else
								circle(imgFiltered, p2, 1, CV_RGB(0, 255, 255), -1);

							// Combined Intensity of the subpixel
							int pixelIntensity = subpixSampleSafe(imgFiltered, subPixel);

							// Converte from index to pixel coordinate
							// m (Column, real) -> -1,0,1 but we need to map to 0,1,2 -> add 1 to 0..2
							int w = m + 1;

							// n (Row, real) -> add stripelenght >> 1 to shift to 0..stripeLength
							// n=0 -> -length/2, n=length/2 -> 0 ........ + length/2
							int h = n + (strip.stripeLength >> 1);

							// Set pointer to correct position and safe subpixel intensity
							imagePixelStripe.at<uchar>(h, w) = (uchar)pixelIntensity;

							// Added in Sheet 3 - Ex7 (a) End *****************************************************************
						}
					}

					// Added in Sheet 3 - Ex7 (b) Start *****************************************************************
					// Use sobel operator on stripe

					// ( -1 , -2, -1 )
					// (  0 ,  0,  0 )
					// (  1 ,  2,  1 )

					/*

					// The first and last row must be excluded from the sobel calculation because they have no top or bottom neighbors
					vector<double> sobelValues(strip.stripeLength - 2.);

					// To use the kernel we start with the second row (n) and stop before the last one
					for (int n = 1; n < (strip.stripeLength - 1); n++) {
						// Take the intensity value from the stripe 
						unsigned char* stripePtr = &(imagePixelStripe.at<uchar>(n - 1, 0));

						// Calculation of the gradient with the sobel for the first row
						double r1 = -stripePtr[0] - 2. * stripePtr[1] - stripePtr[2];

						// r2 -> Is equal to 0 because of sobel

						// Go two lines for the thrid line of the sobel, step = size of the data type, here uchar
						stripePtr += 2 * imagePixelStripe.step;

						// Calculation of the gradient with the sobel for the third row
						double r3 = stripePtr[0] + 2. * stripePtr[1] + stripePtr[2];

						// Writing the result into our sobel value vector
						unsigned int ti = n - 1;
						sobelValues[ti] = r1 + r3;
					}
					*/

					// Simple sobel over the y direction
					Mat grad_y;
					Sobel(imagePixelStripe, grad_y, CV_8UC1, 0, 1);

					double maxIntensity = -1;
					int maxIntensityIndex = 0;

					// Finding the max value
					for (int n = 0; n < strip.stripeLength - 2; ++n) {
						if (grad_y.at<uchar>(n,1) > maxIntensity) {
							maxIntensity = grad_y.at<uchar>(n, 1);
							maxIntensityIndex = n;
						}
					}

					// Added in Sheet 3 - Ex7 (b) End *****************************************************************

					// Added in Sheet 3 - Ex7 (d) Start *****************************************************************

					// f(x) slide 7 -> y0 .. y1 .. y2
					double y0, y1, y2;

					// Point before and after
					unsigned int max1 = maxIntensityIndex - 1, max2 = maxIntensityIndex + 1;

					// If the index is at the border we are out of the stripe, then we will take 0
					y0 = (maxIntensityIndex <= 0) ? 0 : grad_y.at<uchar>(max1,1);
					y1 = grad_y.at<uchar>(maxIntensityIndex,1);
					// If we are going out of the array of the sobel values
					y2 = (maxIntensityIndex >= strip.stripeLength - 3) ? 0 : grad_y.at<uchar>(max2,1);

					// Formula for calculating the x-coordinate of the vertex of a parabola, given 3 points with equal distances 
					// (xv means the x value of the vertex, d the distance between the points): 
					// xv = x1 + (d / 2) * (y2 - y0)/(2*y1 - y0 - y2)

					// d = 1 because of the normalization and x1 will be added later
					double pos = (y2 - y0) / (4 * y1 - 2 * y0 - 2 * y2);

					// What happens when there is no solution -> /0 or Number == other Number
					// If the found pos is not a number -> there is no solution
					if (isnan(pos)) {
						continue;
					}
					// Check if there is a solution to the calculation, cool trick
					/*if (pos != pos) {
						// Value is not a number -> NAN
						continue;
					}*/

					// Exact point with subpixel accuracy
					Point2d edgeCenter;

					// Where is the edge (max gradient) in the picture?
					int maxIndexShift = maxIntensityIndex - (strip.stripeLength >> 1);

					// Find the original edgepoint -> Is the pixel point at the top or bottom?
					edgeCenter.x = (double)p.x + (((double)maxIndexShift + pos) * strip.stripeVecY.x);
					edgeCenter.y = (double)p.y + (((double)maxIndexShift + pos) * strip.stripeVecY.y);

					// Highlight the subpixel with blue color
					circle(imgFiltered, edgeCenter, 2, CV_RGB(0, 0, 255), -1);

					// Added in Sheet 3 - Ex7 (d) End *****************************************************************

					// Added in Sheet 3 - Ex7 (c) Start *****************************************************************

					// Draw the stripe in the image
					if (isFirstStripe) {
						Mat iplTmp;
						// The intensity differences on the stripe
						resize(imagePixelStripe, iplTmp, Size(100,300));
						
						imshow(stripWindow, iplTmp);
						isFirstStripe = false;
					}

					// Added in Sheet 3 - Ex7 (c) End *****************************************************************
				}
			}

			// -----------------------------

			// -------------------------------------------------
		}

		imshow(contoursWindow, imgFiltered);
		isFirstStripe = true;

		if (waitKey(10) == 27) {
			break;
		}
	}

	destroyWindow(contoursWindow);

	return 0;
}

