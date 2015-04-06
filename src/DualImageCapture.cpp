//============================================================================
// Name        : DualImageCapture.cpp
// Author      : Zack Kirkendoll
// Version     :
// Copyright   : 
// Description : Dual Camera, Single Image Pair Capture in C++, Ansi-style
//============================================================================

#include <mvIMPACT_acquire.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <stdio.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <errno.h>

#include <opencv2/core/core.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//#include <opencv2/opencv.hpp>


using namespace std;
using namespace mvIMPACT::acquire;
using namespace cv;

#define PRESS_A_KEY             \
    getchar();
using namespace std;
using namespace mvIMPACT::acquire;
#   define NO_DISPLAY
#   include <stdint.h>
#   include <stdio.h>
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef bool BOOLEAN;

#   ifdef __GNUC__
#       define BMP_ATTR_PACK __attribute__((packed)) __attribute__ ((aligned (2)))
#   else
#       define BMP_ATTR_PACK
#   endif // #ifdef __GNUC__

typedef struct tagRGBQUAD
{
    BYTE    rgbBlue;
    BYTE    rgbGreen;
    BYTE    rgbRed;
    BYTE    rgbReserved;
} BMP_ATTR_PACK RGBQUAD;

typedef struct tagBITMAPINFOHEADER
{
    DWORD  biSize;
    LONG   biWidth;
    LONG   biHeight;
    WORD   biPlanes;
    WORD   biBitCount;
    DWORD  biCompression;
    DWORD  biSizeImage;
    LONG   biXPelsPerMeter;
    LONG   biYPelsPerMeter;
    DWORD  biClrUsed;
    DWORD  biClrImportant;
} BMP_ATTR_PACK BITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct tagBITMAPFILEHEADER
{
    WORD    bfType;
    DWORD   bfSize;
    WORD    bfReserved1;
    WORD    bfReserved2;
    DWORD   bfOffBits;
} BMP_ATTR_PACK BITMAPFILEHEADER, *PBITMAPFILEHEADER;


//-----------------------------------------------------------------------------
int SaveBMP( const string& filename, const char* pdata, int XSize, int YSize, int pitch, int bitsPerPixel )
//------------------------------------------------------------------------------
{
    static const WORD PALETTE_ENTRIES = 256;

    if( pdata )
    {
        FILE* pFile = fopen( filename.c_str(), "wb" );
        if( pFile )
        {
            BITMAPINFOHEADER    bih;
            BITMAPFILEHEADER    bfh;
            WORD                linelen = static_cast<WORD>( ( XSize * bitsPerPixel + 31 ) / 32 * 4 );  // DWORD aligned
            int                 YPos;
            int                 YStart = 0;

            memset( &bfh, 0, sizeof( BITMAPFILEHEADER ) );
            memset( &bih, 0, sizeof( BITMAPINFOHEADER ) );
            bfh.bfType          = 0x4d42;
            bfh.bfSize          = sizeof( bih ) + sizeof( bfh ) + sizeof( RGBQUAD ) * PALETTE_ENTRIES + static_cast<LONG>( linelen ) * static_cast<LONG>( YSize );
            bfh.bfOffBits       = sizeof( bih ) + sizeof( bfh ) + sizeof( RGBQUAD ) * PALETTE_ENTRIES;
            bih.biSize          = sizeof( bih );
            bih.biWidth         = XSize;
            bih.biHeight        = YSize;
            bih.biPlanes        = 1;
            bih.biBitCount      = static_cast<WORD>( bitsPerPixel );
            bih.biSizeImage     = static_cast<DWORD>( linelen ) * static_cast<DWORD>( YSize );

            if( ( fwrite( &bfh, sizeof( bfh ), 1, pFile ) == 1 ) && ( fwrite( &bih, sizeof( bih ), 1, pFile ) == 1 ) )
            {
                RGBQUAD rgbQ;
                for( int i = 0; i < PALETTE_ENTRIES; i++ )
                {
                    rgbQ.rgbRed      = static_cast<BYTE>( i );
                    rgbQ.rgbGreen    = static_cast<BYTE>( i );
                    rgbQ.rgbBlue     = static_cast<BYTE>( i );
                    rgbQ.rgbReserved = static_cast<BYTE>( 0 );
                    fwrite( &rgbQ, sizeof( rgbQ ), 1, pFile );
                }

                for( YPos = YStart + YSize - 1; YPos >= YStart; YPos-- )
                {
                    if( fwrite( &pdata[YPos * pitch], linelen, 1, pFile ) != 1 )
                    {
                        cout << "SaveBmp: ERR_WRITE_FILE: " << filename << endl;
                    }
                }
            }
            else
            {
                cout << "SaveBmp: ERR_WRITE_FILE: " << filename << endl;
            }
            fclose( pFile );
        }
        else
        {
            cout << "SaveBmp: ERR_CREATE_FILE: " << filename << endl;
        }
    }
    else
    {
        cout << "SaveBmp: ERR_DATA_INVALID:" << filename << endl;
    }
    return 0;
}

//-----------------------------------------------------------------------------
class ThreadParameter
//-----------------------------------------------------------------------------
{
    Device*         m_pDev;
    volatile bool   m_boTerminateThread;
public:
    ThreadParameter( Device* pDev ) : m_pDev( pDev ), m_boTerminateThread( false ) {}
    Device* device( void ) const
    {
        return m_pDev;
    }
    bool    terminated( void ) const
    {
        return m_boTerminateThread;
    }
    void    terminateThread( void )
    {
        m_boTerminateThread = true;
    }
};

//-----------------------------------------------------------------------------
int main( int /*argc*/, char* /*argv*/[] )
//-----------------------------------------------------------------------------
{
	DeviceManager devMgr;
	unsigned int left_camera, right_camera;
	const unsigned int devCnt = devMgr.deviceCount();
	if( devCnt < 2 )
	{
		cout << devCnt << " cameras found. We require at least 2 to do stereo vision" << endl;
		return 0;
	}
	if(!devMgr[0]->serial.read().compare("30000431")) //30000431 is the right camera in the test stand
	{
		right_camera = 0;
		left_camera = 1;
	}else
	{
		right_camera = 1;
		left_camera = 0;
	}
//	vector<ThreadParameter*> threadParams;
//	for( unsigned int i = 0; i < devCnt; i++ )
//	{
//		threadParams.push_back( new ThreadParameter( devMgr[i] ) );
//		cout << devMgr[i]->family.read() << "(" << devMgr[i]->serial.read() << ")" << endl;
//	}

	cout << "Press return to end the acquisition( the initialisation of the devices might take some time )" << endl;
	//for( unsigned int i = 0; i < devCnt; i++ ){
	try
	{
		devMgr[right_camera]->open();
//		threadParams[i]->device()->open();
	}
	catch( const ImpactAcquireException& e )
	{
		// this e.g. might happen if the same device is already opened in another process...
		cout << "An error occurred while opening the right camera (device " << devMgr[right_camera]->serial.read()
			 << ") (error code: " << e.getErrorCode() << "(" << e.getErrorCodeAsString() << ")). Terminating thread." << endl
			 << "Press [ENTER] to end the application..."
			 << endl;
		PRESS_A_KEY
		return 0;
	}
	try
	{
		devMgr[left_camera]->open();
//		threadParams[i]->device()->open();
	}
	catch( const ImpactAcquireException& e )
	{
		// this e.g. might happen if the same device is already opened in another process...
		cout << "An error occurred while opening the left camera (device " << devMgr[left_camera]->serial.read()
			 << ") (error code: " << e.getErrorCode() << "(" << e.getErrorCodeAsString() << ")). Terminating thread." << endl
			 << "Press [ENTER] to end the application..."
			 << endl;
		PRESS_A_KEY
		return 0;
	}
	// now all threads will start running...
	//}
	cout << "Acquisition Complete" <<endl;

	// establish access to the statistic properties
	Statistics right_camera_statistics( devMgr[right_camera]);
	Statistics left_camera_statistics( devMgr[left_camera] );
	// create an interface to the device found
	FunctionInterface right_camera_fi( devMgr[right_camera] );
	FunctionInterface left_camera_fi( devMgr[left_camera] );

	//prefill the capture queue. There can be more then 1 queue for some device, but for this sample
	// we will work with the default capture queue. If a device supports more then one capture or result
	// queue, this will be stated in the manual. If nothing is set about it, the device supports one
	// queue only. Request as many images as possible. If there are no more free requests 'DEV_NO_FREE_REQUEST_AVAILABLE'
	// will be returned by the driver.
	int result = DMR_NO_ERROR;
	SystemSettings right_camera_ss( devMgr[right_camera] );
	SystemSettings left_camera_ss( devMgr[left_camera] );
	const int RIGHT_REQUEST_COUNT = right_camera_ss.requestCount.read();
	const int LEFT_REQUEST_COUNT = left_camera_ss.requestCount.read();
	for( int i = 0; i < RIGHT_REQUEST_COUNT; i++ )
	{
	   result = right_camera_fi.imageRequestSingle();
	   if( result != DMR_NO_ERROR )
	   {
		   cout << "Error while filling the request queue: " << ImpactAcquireException::getErrorCodeAsString( result ) << endl;
	   }
	}
	for( int i = 0; i < LEFT_REQUEST_COUNT; i++ )
	{
		result = left_camera_fi.imageRequestSingle();
		if( result != DMR_NO_ERROR )
		{
			cout << "Error while filling the request queue: " << ImpactAcquireException::getErrorCodeAsString( result ) << endl;
		}
	}

	//TAKE PICTURE
	// run thread loop
	const Request* pRequest = 0;
	const unsigned int timeout_ms = 2000;   // USB 1.1 on an embedded system needs a large timeout for the first image
	int requestNr = INVALID_ID;
	// This next comment is valid once we have a display:
	// we always have to keep at least 2 images as the display module might want to repaint the image, thus we
	// can't free it unless we have a assigned the display to a new buffer.
	int lastRequestNr = INVALID_ID;

	//Mat image(Size(1280,960), CV_8UC1);
	//Mat image(Size(pRequest->imageWidth.read(), pRequest->imageHeight.read()), CV_8UC1, reinterpret_cast<char*>( pRequest->imageData.read() ), Mat::AUTO_STEP);
	namedWindow("LeftImage", WINDOW_NORMAL);
	moveWindow("LeftImage", 705, 20);
	resizeWindow("LeftImage", 640, 480);
	namedWindow("RightImage", WINDOW_NORMAL);
	moveWindow("RightImage", 0, 20);
	resizeWindow("RightImage", 640, 480);
	namedWindow("LeftUImage", WINDOW_NORMAL);
	moveWindow("LeftUImage", 705, 550);
	resizeWindow("LeftUImage", 640, 480);
	namedWindow("RightUImage", WINDOW_NORMAL);
	moveWindow("RightUImage", 0, 550);
	resizeWindow("RightUImage", 640, 480);
	//bool first_time = true;
	int returnkey;
	string left_camera_file_name;
	string right_camera_file_name;
	unsigned int image_num = 0;
	Mat right_image;
	Mat left_image;

	//Pull in the camera calibration
	Mat CM1;// = Mat(3, 3, CV_64FC1);
	Mat CM2;// = Mat(3, 3, CV_64FC1);
	Mat D1, D2;
	Mat R, T, E, F;

	//Pull in the intrinsic camera calibration for the left camera
	FileStorage fs("/home/loyd-hook/0Projects/SV/software/eclipse_ws/CameraCalibration/Debug/mycalib.yml", FileStorage::READ);
	if(!fs.isOpened())
	{
		printf("Failed to open file mycalib on left image");
		exit (-1);
	}
	fs["M1"] >> CM1;
	fs["D1"] >> D1;
	fs["M2"] >> CM2;
	fs["D2"] >> D2;
	fs.release();

	for(int in = 0;;in++){
	//Right CAMERA
	// wait for results from the default capture queue
	requestNr = right_camera_fi.imageRequestWaitFor( timeout_ms );


	//Mat image(Size(1280,960), CV_8UC1);
	if( right_camera_fi.isRequestNrValid( requestNr ) )
	{
		pRequest = right_camera_fi.getRequest( requestNr );
		if( pRequest->isOK() )
		{
			// here we will save all the pictures as .bmp
			// we will also only keep the most recent 10 pictures by deleting the previous ones
			// this uses stringstream to store string+integer
			// converts strings to char array in order to remove the file

			//char mystring2[4];
			//spsetwrintf(mystring2, "%d", in);
			//mystring = "/home/loyd-hook/0Projects/SV/Images/DevelopmentPics00/" + threadParams[0]->device()->serial.read() + "_" + mystring2  + ".jpg";
			//cout << mystring <<endl;
			/*mystring2 << "/home/zkirkendoll/Documents/Images/Continuous2/test" << pThreadParameter->device()->serial.read() <<"_"<<cnt << ".bmp";

			char test[100];
			for(unsigned int i=0; i<=mystring2.str().size();i++)
			{
				test[i]=mystring2.str()[i];
			}
			//remove(test);*/
			Mat image(Size(pRequest->imageWidth.read(), pRequest->imageHeight.read()), CV_8UC1, pRequest->imageData.read(), Mat::AUTO_STEP);
			image.copyTo(right_image);
			Mat uimage;
			undistort(right_image, uimage, CM2, D2);

			//undistort the image and show that one instead...or the original
			imshow("RightImage", image);
			imshow("RightUImage", uimage);

			//printf("press any key to continue...");


			//waitKey(0);
			//destroyWindow("Image");

		}
		else
		{
			cout << "Error: " << pRequest->requestResult.readS() << endl;
		}
		if( right_camera_fi.isRequestNrValid( lastRequestNr ) )
		{
			// this image has been displayed thus the buffer is no longer needed...
			right_camera_fi.imageRequestUnlock( lastRequestNr );
		}
		lastRequestNr = requestNr;
		// send a new image request into the capture queue
		right_camera_fi.imageRequestSingle();
	}
	else
	{
		// If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
		// additional information under TDMR_ERROR in the interface reference (
		cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ", device " << devMgr[right_camera]->serial.read() << ")"
			 << ", timeout value too small?" << endl;
	}
	//Left camera
	//for(int in = 0;;in++){
	//FIRST CAMERA
	// wait for results from the default capture queue
	requestNr = left_camera_fi.imageRequestWaitFor( timeout_ms );


	//Mat image(Size(1280,960), CV_8UC1);
	if( left_camera_fi.isRequestNrValid( requestNr ) )
	{
		pRequest = left_camera_fi.getRequest( requestNr );
		if( pRequest->isOK() )
		{
			// here we will save all the pictures as .bmp
			// we will also only keep the most recent 10 pictures by deleting the previous ones
			// this uses stringstream to store string+integer
			// converts strings to char array in order to remove the file

			//char mystring2[4];
			//sprintf(mystring2, "%d", in);
			//mystring = "/home/loyd-hook/0Projects/SV/Images/DevelopmentPics00/" + threadParams[0]->device()->serial.read() + "_" + mystring2  + ".jpg";
			//cout << mystring <<endl;
			/*mystring2 << "/home/zkirkendoll/Documents/Images/Continuous2/test" << pThreadParameter->device()->serial.read() <<"_"<<cnt << ".bmp";

			char test[100];
			for(unsigned int i=0; i<=mystring2.str().size();i++)
			{
				test[i]=mystring2.str()[i];
			}
			//remove(test);*/
			Mat image(Size(pRequest->imageWidth.read(), pRequest->imageHeight.read()), CV_8UC1, pRequest->imageData.read(), Mat::AUTO_STEP);
			image.copyTo(left_image);
			Mat uimage;
			undistort(left_image, uimage, CM1, D1);

			//undistort the image and show that one instead...or the original
			imshow("LeftImage", image);
			imshow("LeftUImage", uimage);

			//printf("press any key to continue...");


			//waitKey(0);
			//destroyWindow("Image");

		}
		else
		{
			cout << "Error: " << pRequest->requestResult.readS() << endl;
		}
		if( left_camera_fi.isRequestNrValid( lastRequestNr ) )
		{
			// this image has been displayed thus the buffer is no longer needed...
			left_camera_fi.imageRequestUnlock( lastRequestNr );
		}
		lastRequestNr = requestNr;
		// send a new image request into the capture queue
		left_camera_fi.imageRequestSingle();
	}
	else
	{
		// If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
		// additional information under TDMR_ERROR in the interface reference (
		cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ", device " << devMgr[left_camera]->serial.read() << ")"
			 << ", timeout value too small?" << endl;
	}
	//Process
	returnkey = waitKey(50);
	if(returnkey > 0)
	{
		cout << "Return Key Pressed is " << returnkey << endl;
	}
	if(returnkey == 1048691) //'s' key
	{
		cout << "Right Image height is " << right_image.rows << " width " << right_image.cols << endl
			 << "Info from " << devMgr[right_camera]->serial.read()
			 << ": " << right_camera_statistics.framesPerSecond.name() << ": " << right_camera_statistics.framesPerSecond.readS()
			 << ", " << right_camera_statistics.errorCount.name() << ": " << right_camera_statistics.errorCount.readS()
			 << ", " << right_camera_statistics.captureTime_s.name() << ": " << right_camera_statistics.captureTime_s.readS() << endl;

		cout << "Image height is " << left_image.rows << " width " << left_image.cols << endl
					 << "Info from " << devMgr[left_camera]->serial.read()
					 << ": " << left_camera_statistics.framesPerSecond.name() << ": " << left_camera_statistics.framesPerSecond.readS()
					 << ", " << left_camera_statistics.errorCount.name() << ": " << left_camera_statistics.errorCount.readS()
					 << ", " << left_camera_statistics.captureTime_s.name() << ": " << left_camera_statistics.captureTime_s.readS() << endl;
	}
	else if(returnkey == 1048688) // 'p' ket
	{
		cout << "Images are paused press any key to resume" << endl;
		waitKey(0);
	}
	else if(returnkey == 1048586) //enter key
	{
		ostringstream ssr, ssl;
		ssl << "/home/loyd-hook/0Projects/SV/Images/DevelopmentPics06/SD06_O" << setw(4) << setfill('0') << image_num << "_0.bmp"; // see wiki for naming convention
		left_camera_file_name = ssl.str();
		imwrite(left_camera_file_name, left_image);
		cout << "Left image stored at" << ssl.str() << endl;

		ssr << "/home/loyd-hook/0Projects/SV/Images/DevelopmentPics06/SD06_O" << setw(4) << setfill('0') << image_num << "_1.bmp";
		right_camera_file_name = ssr.str();
		imwrite(right_camera_file_name, right_image);
		cout << "Right image stored at" << ssr.str() << endl;

		image_num++;
	}
	else if(returnkey == 1048603) //esc key
	{
		break;
	}
	//break;
	}
	//SECOND CAMERA
	// wait for results from the default capture queue
//	requestNr = fi1.imageRequestWaitFor( timeout_ms );
//	if( fi1.isRequestNrValid( requestNr ) )
//	{
//		pRequest = fi1.getRequest( requestNr );
//		if( pRequest->isOK() )
//		{
//			// here we will save all the pictures as .bmp
//			// we will also only keep the most recent 10 pictures by deleting the previous ones
//			// this uses stringstream to store string+integer
//			// converts strings to char array in order to remove the file
//			ostringstream mystring,mystring2;
//			mystring << "/home/loyd-hook/0Projects/SV/StereoPairJunk/test/" << threadParams[1]->device()->serial.read() <<"_"<< "1.bmp";
//			/*mystring2 << "/home/zkirkendoll/Documents/Images/Continuous2/test" << pThreadParameter->device()->serial.read() <<"_"<<cnt << ".bmp";
//			char test[100];
//			for(unsigned int i=0; i<=mystring2.str().size();i++)
//			{
//				test[i]=mystring2.str()[i];
//			}
//			//remove(test);*/
//			SaveBMP( mystring.str(), reinterpret_cast<char*>( pRequest->imageData.read() ), pRequest->imageWidth.read(), pRequest->imageHeight.read(), pRequest->imageLinePitch.read(), pRequest->imagePixelPitch.read() * 8 );
//			// here we can display some statistical information every 100th image
//			cout << "Info from " << threadParams[1]->device()->serial.read()
//				 << ": " << statistics1.framesPerSecond.name() << ": " << statistics1.framesPerSecond.readS()
//				 << ", " << statistics1.errorCount.name() << ": " << statistics1.errorCount.readS()
//				 << ", " << statistics1.captureTime_s.name() << ": " << statistics1.captureTime_s.readS() << endl;
//		}
//		else
//		{
//			cout << "Error: " << pRequest->requestResult.readS() << endl;
//		}
//		if( fi1.isRequestNrValid( lastRequestNr ) )
//		{
//			// this image has been displayed thus the buffer is no longer needed...
//			fi1.imageRequestUnlock( lastRequestNr );
//		}
//		lastRequestNr = requestNr;
//		// send a new image request into the capture queue
//		fi1.imageRequestSingle();
//	}
//	else
//	{
//		// If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
//		// additional information under TDMR_ERROR in the interface reference (
//		cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ", device " << threadParams[1]->device()->serial.read() << ")"
//			 << ", timeout value too small?" << endl;
//	}

	// free the last potential locked request
	if( right_camera_fi.isRequestNrValid( requestNr ) )
	{
		right_camera_fi.imageRequestUnlock( requestNr );
	}
	// clear the request queue
	right_camera_fi.imageRequestReset( 0, 0 );
	// free the last potential locked request
//	if( fi1.isRequestNrValid( requestNr ) )
//	{
//		fi1.imageRequestUnlock( requestNr );
//	}
//	// clear the request queue
//	fi1.imageRequestReset( 0, 0 );
//	printf("press any key to continue...");
	fflush(stdout);
//	waitKey();
	printf("\n");
	cout << "Image Acquisition Complete!" << endl;
	return 0;
}
