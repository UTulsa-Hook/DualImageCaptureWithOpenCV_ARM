//============================================================================
// Name        : DualImageCapture.cpp
// Author      : Zack Kirkendoll
// Version     :
// Copyright   : 
// Description : Dual Camera, Single Image Pair Capture in C++, Ansi-style
//============================================================================

#include <mvIMPACT_acquire.h>
#include <iostream>
#include <string>
#include <sstream>
#include <stdio.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <errno.h>


using namespace std;
using namespace mvIMPACT::acquire;

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
	const unsigned int devCnt = devMgr.deviceCount();
	if( devCnt == 0 )
	{
		cout << "No MATRIX VISION device found! Unable to continue!" << endl;
		return 0;
	}

	vector<ThreadParameter*> threadParams;
	for( unsigned int i = 0; i < devCnt; i++ )
	{
		threadParams.push_back( new ThreadParameter( devMgr[i] ) );
		cout << devMgr[i]->family.read() << "(" << devMgr[i]->serial.read() << ")" << endl;
	}

	cout << "Press return to end the acquisition( the initialisation of the devices might take some time )" << endl;
	for( unsigned int i = 0; i < devCnt; i++ ){
	try
		{
			threadParams[i]->device()->open();
		}
		catch( const ImpactAcquireException& e )
		{
			// this e.g. might happen if the same device is already opened in another process...
			cout << "An error occurred while opening the device " << threadParams[i]->device()->serial.read()
				 << "(error code: " << e.getErrorCode() << "(" << e.getErrorCodeAsString() << ")). Terminating thread." << endl
				 << "Press [ENTER] to end the application..."
				 << endl;
			PRESS_A_KEY
			return 0;
		}
	// now all threads will start running...

	}
	cout << "Acquisition Complete" <<endl;

	// establish access to the statistic properties
	Statistics statistics0( threadParams[0]->device() );
	Statistics statistics1( threadParams[1]->device() );
	// create an interface to the device found
	FunctionInterface fi0( threadParams[0]->device() );
	FunctionInterface fi1( threadParams[1]->device() );

	//prefill the capture queue. There can be more then 1 queue for some device, but for this sample
	// we will work with the default capture queue. If a device supports more then one capture or result
	// queue, this will be stated in the manual. If nothing is set about it, the device supports one
	// queue only. Request as many images as possible. If there are no more free requests 'DEV_NO_FREE_REQUEST_AVAILABLE'
	// will be returned by the driver.
	int result = DMR_NO_ERROR;
	SystemSettings ss0( threadParams[0]->device() );
	SystemSettings ss1( threadParams[1]->device() );
	const int REQUEST_COUNT0 = ss0.requestCount.read();
	const int REQUEST_COUNT1 = ss1.requestCount.read();
	for( int i = 0; i < REQUEST_COUNT0; i++ )
	{
	   result = fi0.imageRequestSingle();
	   if( result != DMR_NO_ERROR )
	   {
		   cout << "Error while filling the request queue: " << ImpactAcquireException::getErrorCodeAsString( result ) << endl;
	   }
	}
	for( int i = 0; i < REQUEST_COUNT1; i++ )
	{
		result = fi1.imageRequestSingle();
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


	//FIRST CAMERA
	// wait for results from the default capture queue
	requestNr = fi0.imageRequestWaitFor( timeout_ms );
	if( fi0.isRequestNrValid( requestNr ) )
	{
		pRequest = fi0.getRequest( requestNr );
		if( pRequest->isOK() )
		{
			// here we will save all the pictures as .bmp
			// we will also only keep the most recent 10 pictures by deleting the previous ones
			// this uses stringstream to store string+integer
			// converts strings to char array in order to remove the file
			ostringstream mystring,mystring2;
			mystring << "/home/zkirkendoll/Documents/Images/DualImageCapture/C" << threadParams[0]->device()->serial.read() <<"_"<< "1.bmp";
			/*mystring2 << "/home/zkirkendoll/Documents/Images/Continuous2/test" << pThreadParameter->device()->serial.read() <<"_"<<cnt << ".bmp";
			char test[100];
			for(unsigned int i=0; i<=mystring2.str().size();i++)
			{
				test[i]=mystring2.str()[i];
			}
			//remove(test);*/
			SaveBMP( mystring.str(), reinterpret_cast<char*>( pRequest->imageData.read() ), pRequest->imageWidth.read(), pRequest->imageHeight.read(), pRequest->imageLinePitch.read(), pRequest->imagePixelPitch.read() * 8 );
			// here we can display some statistical information every 100th image
			cout << "Info from " << threadParams[0]->device()->serial.read()
				 << ": " << statistics0.framesPerSecond.name() << ": " << statistics0.framesPerSecond.readS()
				 << ", " << statistics0.errorCount.name() << ": " << statistics0.errorCount.readS()
				 << ", " << statistics0.captureTime_s.name() << ": " << statistics0.captureTime_s.readS() << endl;

		}
		else
		{
			cout << "Error: " << pRequest->requestResult.readS() << endl;
		}
		if( fi0.isRequestNrValid( lastRequestNr ) )
		{
			// this image has been displayed thus the buffer is no longer needed...
			fi0.imageRequestUnlock( lastRequestNr );
		}
		lastRequestNr = requestNr;
		// send a new image request into the capture queue
		fi0.imageRequestSingle();
	}
	else
	{
		// If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
		// additional information under TDMR_ERROR in the interface reference (
		cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ", device " << threadParams[0]->device()->serial.read() << ")"
			 << ", timeout value too small?" << endl;
	}
	//SECOND CAMERA
	// wait for results from the default capture queue
	requestNr = fi1.imageRequestWaitFor( timeout_ms );
	if( fi1.isRequestNrValid( requestNr ) )
	{
		pRequest = fi1.getRequest( requestNr );
		if( pRequest->isOK() )
		{
			// here we will save all the pictures as .bmp
			// we will also only keep the most recent 10 pictures by deleting the previous ones
			// this uses stringstream to store string+integer
			// converts strings to char array in order to remove the file
			ostringstream mystring,mystring2;
			mystring << "/home/zkirkendoll/Documents/Images/DualImageCapture/C" << threadParams[1]->device()->serial.read() <<"_"<< "1.bmp";
			/*mystring2 << "/home/zkirkendoll/Documents/Images/Continuous2/test" << pThreadParameter->device()->serial.read() <<"_"<<cnt << ".bmp";
			char test[100];
			for(unsigned int i=0; i<=mystring2.str().size();i++)
			{
				test[i]=mystring2.str()[i];
			}
			//remove(test);*/
			SaveBMP( mystring.str(), reinterpret_cast<char*>( pRequest->imageData.read() ), pRequest->imageWidth.read(), pRequest->imageHeight.read(), pRequest->imageLinePitch.read(), pRequest->imagePixelPitch.read() * 8 );
			// here we can display some statistical information every 100th image
			cout << "Info from " << threadParams[1]->device()->serial.read()
				 << ": " << statistics1.framesPerSecond.name() << ": " << statistics1.framesPerSecond.readS()
				 << ", " << statistics1.errorCount.name() << ": " << statistics1.errorCount.readS()
				 << ", " << statistics1.captureTime_s.name() << ": " << statistics1.captureTime_s.readS() << endl;
		}
		else
		{
			cout << "Error: " << pRequest->requestResult.readS() << endl;
		}
		if( fi1.isRequestNrValid( lastRequestNr ) )
		{
			// this image has been displayed thus the buffer is no longer needed...
			fi1.imageRequestUnlock( lastRequestNr );
		}
		lastRequestNr = requestNr;
		// send a new image request into the capture queue
		fi1.imageRequestSingle();
	}
	else
	{
		// If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
		// additional information under TDMR_ERROR in the interface reference (
		cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ", device " << threadParams[1]->device()->serial.read() << ")"
			 << ", timeout value too small?" << endl;
	}

	// free the last potential locked request
	if( fi0.isRequestNrValid( requestNr ) )
	{
		fi0.imageRequestUnlock( requestNr );
	}
	// clear the request queue
	fi0.imageRequestReset( 0, 0 );
	// free the last potential locked request
	if( fi1.isRequestNrValid( requestNr ) )
	{
		fi1.imageRequestUnlock( requestNr );
	}
	// clear the request queue
	fi1.imageRequestReset( 0, 0 );

	cout << "Image Acquisition Complete!" << endl;
	return 0;
}
