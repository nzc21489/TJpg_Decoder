/*
TJpg_Decoder.h

JPEG Decoder for Arduino using TJpgDec:
http://elm-chan.org/fsw/tjpgd/00index.html

Incorporated into an Arduino library by Bodmer 18/10/19

Latest version here:
https://github.com/Bodmer/TJpg_Decoder
*/

#ifndef TJpg_Decoder_H
  #define TJpg_Decoder_H

  #include "User_Config.h"
  // #include "Arduino.h"
  #include "tjpgd.h"

  #include <string>

  using namespace std;

  #if defined (ESP8266) || defined (ESP32)
    #include <pgmspace.h>

    #define TJPGD_LOAD_SPIFFS
    #define FS_NO_GLOBALS
    #include <FS.h>

    #ifdef ESP32
      #include "SPIFFS.h" // ESP32 only
    #endif
  #endif

#if defined (TJPGD_LOAD_SD_LIBRARY)
  // #include <SD.h>
  #include "ff.h"
#endif


enum {
	TJPG_ARRAY = 0,
	TJPG_FS_FILE,
	TJPG_SD_FILE,
  TJPG_SD_AUDIO_FILE
};

//------------------------------------------------------------------------------

typedef bool (*SketchCallback)(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *data);

class TJpg_Decoder {

private:
#if defined (TJPGD_LOAD_SD_LIBRARY)
  FIL jpgSdFile;
#endif

#ifdef TJPGD_LOAD_SPIFFS
  fs::FIL jpgFile;
#endif

public:

  TJpg_Decoder();
  ~TJpg_Decoder();

  static int jd_output(JDEC* jdec, void* bitmap, JRECT* jrect);
  static unsigned int jd_input(JDEC* jdec, uint8_t* buf, unsigned int len);

  void setJpgScale(uint8_t scale);
  void setCallback(SketchCallback sketchCallback);


#if defined (TJPGD_LOAD_SD_LIBRARY) || defined (TJPGD_LOAD_SPIFFS)
  JRESULT drawJpg (int32_t x, int32_t y, const char *pFilename);
  JRESULT drawJpg (int32_t x, int32_t y, const string& pFilename);

  JRESULT getJpgSize(uint16_t *w, uint16_t *h, const char *pFilename);
  JRESULT getJpgSize(uint16_t *w, uint16_t *h, const string& pFilename);
#endif

#if defined (TJPGD_LOAD_SD_LIBRARY)
  JRESULT drawSdJpg (int32_t x, int32_t y, const char *pFilename);
  JRESULT drawSdJpg (int32_t x, int32_t y, const string& pFilename);
  JRESULT drawSdJpg (int32_t x, int32_t y, FIL inFile);

  JRESULT getSdJpgSize(uint16_t *w, uint16_t *h, const char *pFilename);
  JRESULT getSdJpgSize(uint16_t *w, uint16_t *h, const string& pFilename);
  JRESULT getSdJpgSize(uint16_t *w, uint16_t *h, FIL inFile);

  JRESULT drawSdAudioJpg (int32_t x, int32_t y, const char *pFilename, int32_t jpegOffset, int32_t jpegSize);
  JRESULT drawSdAudioJpg (int32_t x, int32_t y, const string& pFilename, int32_t jpegOffset, int32_t jpegSize);
  JRESULT drawSdAudioJpg (int32_t x, int32_t y, FIL inFile, int32_t jpegOffset, int32_t jpegSize);

  JRESULT getSdAudioJpgSize(uint16_t *w, uint16_t *h, const char *pFilename, int32_t jpegOffset, int32_t jpegSize);
  JRESULT getSdAudioJpgSize(uint16_t *w, uint16_t *h, const string& pFilename, int32_t jpegOffset, int32_t jpegSize);
  JRESULT getSdAudioJpgSize(uint16_t *w, uint16_t *h, FIL inFile, int32_t jpegOffset, int32_t jpegSize);
#endif

#ifdef TJPGD_LOAD_SPIFFS
  JRESULT drawFsJpg (int32_t x, int32_t y, const char *pFilename);
  JRESULT drawFsJpg (int32_t x, int32_t y, const string& pFilename);
  JRESULT drawFsJpg (int32_t x, int32_t y, fs::FIL inFile);

  JRESULT getFsJpgSize(uint16_t *w, uint16_t *h, const char *pFilename);
  JRESULT getFsJpgSize(uint16_t *w, uint16_t *h, const string& pFilename);
  JRESULT getFsJpgSize(uint16_t *w, uint16_t *h, fs::FIL inFile);
#endif

  JRESULT drawJpg(int32_t x, int32_t y, const uint8_t array[], uint32_t  array_size);
  JRESULT getJpgSize(uint16_t *w, uint16_t *h, const uint8_t array[], uint32_t  array_size);

  void setSwapBytes(bool swap);

  bool _swap = false;

  const uint8_t* array_data  = nullptr;
  uint32_t array_index = 0;
  uint32_t array_size  = 0;

  // Must align workspace to a 32 bit boundary
  uint8_t workspace[TJPGD_WORKSPACE_SIZE] __attribute__((aligned(4)));

  uint8_t jpg_source = 0;

  int16_t jpeg_x = 0;
  int16_t jpeg_y = 0;

  uint8_t jpgScale = 0;

  SketchCallback tft_output = nullptr;

  TJpg_Decoder *thisPtr = nullptr;

  int32_t jpeg_offset = 0;
  int32_t jpeg_size = 0;

  bool jpeg_start = true;
  uint32_t jpeg_buf_count = 0;
};

extern TJpg_Decoder *TJpgDec;

#endif // TJpg_Decoder_H
