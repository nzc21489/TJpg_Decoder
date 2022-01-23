/*
TJpg_Decoder.cpp

Created by Bodmer 18/10/19

Latest version here:
https://github.com/Bodmer/TJpg_Decoder
*/

#include "TJpg_Decoder.h"
#include <string.h>

// Create a class instance to be used by the sketch (defined as extern in header)
TJpg_Decoder TJpgDec;

/***************************************************************************************
** Function name:           TJpg_Decoder
** Description:             Constructor
***************************************************************************************/
TJpg_Decoder::TJpg_Decoder(){
  // Setup a pointer to this class for static functions
  thisPtr = this;
}

/***************************************************************************************
** Function name:           ~TJpg_Decoder
** Description:             Destructor
***************************************************************************************/
TJpg_Decoder::~TJpg_Decoder(){
  // Bye
}

/***************************************************************************************
** Function name:           setJpgScale
** Description:             Set the reduction scale factor (1, 2, 4 or 8)
***************************************************************************************/
void TJpg_Decoder::setSwapBytes(bool swapBytes){
  _swap = swapBytes;
}

/***************************************************************************************
** Function name:           setJpgScale
** Description:             Set the reduction scale factor (1, 2, 4 or 8)
***************************************************************************************/
void TJpg_Decoder::setJpgScale(uint8_t scaleFactor)
{
  switch (scaleFactor)
  {
    case 1:
      jpgScale = 0;
      break;
    case 2:
      jpgScale = 1;
      break;
    case 4:
      jpgScale = 2;
      break;
    case 8:
      jpgScale = 3;
      break;
    default:
      jpgScale = 0;
  }
}

/***************************************************************************************
** Function name:           setCallback
** Description:             Set the sketch callback function to render decoded blocks
***************************************************************************************/
void TJpg_Decoder::setCallback(SketchCallback sketchCallback)
{
  tft_output = sketchCallback;
}

/***************************************************************************************
** Function name:           jd_input (declared static)
** Description:             Called by tjpgd.c to get more data
***************************************************************************************/
unsigned int TJpg_Decoder::jd_input(JDEC* jdec, uint8_t* buf, unsigned int len)
{
  TJpg_Decoder *thisPtr = TJpgDec.thisPtr;
  jdec = jdec; // Supress warning

  // Handle an array input
  if (thisPtr->jpg_source == TJPG_ARRAY) {
    // Avoid running off end of array
    if (thisPtr->array_index + len > thisPtr->array_size) {
      len = thisPtr->array_size - thisPtr->array_index;
    }

    // If buf is valid then copy len bytes to buffer
    if (buf) memcpy(buf, (const uint8_t *)(thisPtr->array_data + thisPtr->array_index), len);

    // Move pointer
    thisPtr->array_index += len;
  }

#ifdef TJPGD_LOAD_SPIFFS
  // Handle SPIFFS input
  else if (thisPtr->jpg_source == TJPG_FS_FILE) {
    // Check how many bytes are available
    uint32_t bytesLeft = thisPtr->jpgFile.available();
    if (bytesLeft < len) len = bytesLeft;

    if (buf) {
      // Read into buffer, pointer moved as well
      thisPtr->jpgFile.read(buf, len);
    }
    else {
      // Buffer is null, so skip data by moving pointer
      thisPtr->jpgFile.seek(thisPtr->jpgFile.position() + len);
    }
  }
#endif

#if defined (TJPGD_LOAD_SD_LIBRARY)
  // Handle SD library input
  else if (thisPtr->jpg_source == TJPG_SD_FILE) {
    UINT br;
    // Check how many bytes are available
    // uint32_t bytesLeft = thisPtr->jpgSdFile.available();
    uint32_t bytesLeft = f_size(&thisPtr->jpgSdFile);
    if (bytesLeft < len) len = bytesLeft;

    if (buf) {
      // Read into buffer, pointer moved as well
      // thisPtr->jpgSdFile.read(buf, len);
      f_read(&thisPtr->jpgSdFile, buf, len, &br);
    }
    else {
      // Buffer is null, so skip data by moving pointer
      // thisPtr->jpgSdFile.seek(thisPtr->jpgSdFile.position() + len);
      f_lseek(&thisPtr->jpgSdFile, f_tell(&thisPtr->jpgSdFile) + len);
    }
  }
  else if (thisPtr->jpg_source == TJPG_SD_AUDIO_FILE){
    // seek at start
    if (thisPtr->jpeg_start){
      // thisPtr->jpgSdFile.seek(thisPtr->jpgSdFile.position() + thisPtr->jpeg_offset);
      f_lseek(&thisPtr->jpgSdFile, f_tell(&thisPtr->jpgSdFile) + thisPtr->jpeg_offset);
      thisPtr->jpeg_start = false;
    }
    uint32_t bytesLeft = thisPtr->jpeg_size - thisPtr->jpeg_buf_count;
    
    if (bytesLeft < len) len = bytesLeft;

    if (buf) {
      // Read into buffer, pointer moved as well
      // thisPtr->jpgSdFile.read(buf, len);
      UINT br;
      f_read(&thisPtr->jpgSdFile, buf, len, &br);
      thisPtr->jpeg_buf_count += len;
      // printf("jpeg audio buf\n");
      // printf("%d\n", thisPtr->jpgSdFile.position());
    }
    else {
      // Buffer is null, so skip data by moving pointer
      // thisPtr->jpgSdFile.seek(thisPtr->jpgSdFile.position() + len);
      f_lseek(&thisPtr->jpgSdFile, f_tell(&thisPtr->jpgSdFile) + len);
      thisPtr->jpeg_buf_count += len;
    }
  }
#endif

  return len;
}

/***************************************************************************************
** Function name:           jd_output (declared static)
** Description:             Called by tjpgd.c with an image block for rendering
***************************************************************************************/
// Pass image block back to the sketch for rendering, may be a complete or partial MCU
int TJpg_Decoder::jd_output(JDEC* jdec, void* bitmap, JRECT* jrect)
{
  // This is a static function so create a pointer to access other members of the class
  TJpg_Decoder *thisPtr = TJpgDec.thisPtr;

  jdec = jdec; // Supress warning as ID is not used

  // Retrieve rendering parameters and add any offset
  int16_t  x = jrect->left + thisPtr->jpeg_x;
  int16_t  y = jrect->top  + thisPtr->jpeg_y;
  uint16_t w = jrect->right  + 1 - jrect->left;
  uint16_t h = jrect->bottom + 1 - jrect->top;

  // Pass the image block and rendering parameters in a callback to the sketch
  return thisPtr->tft_output(x, y, w, h, (uint16_t*)bitmap);
}


#if defined (TJPGD_LOAD_SD_LIBRARY) || defined (TJPGD_LOAD_SPIFFS)

/***************************************************************************************
** Function name:           drawJpg
** Description:             Draw a named jpg file at x,y (name in char array)
***************************************************************************************/
// Generic file call for SD or SPIFFS, uses leading / to distinguish SPIFFS files
JRESULT TJpg_Decoder::drawJpg(int32_t x, int32_t y, const char *pFilename){

#if defined (ESP8266) || defined (ESP32)
#if defined (TJPGD_LOAD_SD_LIBRARY)
    if (*pFilename == '/')
#endif
    return drawFsJpg(x, y, pFilename);
#endif

#if defined (TJPGD_LOAD_SD_LIBRARY)
    return drawSdJpg(x, y, pFilename);
#endif

    return JDR_INP;
}

/***************************************************************************************
** Function name:           drawJpg
** Description:             Draw a named jpg file at x,y (name in string)
***************************************************************************************/
// Generic file call for SD or SPIFFS, uses leading / to distinguish SPIFFS files
JRESULT TJpg_Decoder::drawJpg(int32_t x, int32_t y, const string& pFilename){

#if defined (ESP8266) || defined (ESP32)
#if defined (TJPGD_LOAD_SD_LIBRARY)
    if (pFilename.charAt(0) == '/')
#endif
    return drawFsJpg(x, y, pFilename);
#endif

#if defined (TJPGD_LOAD_SD_LIBRARY)
    return drawSdJpg(x, y, pFilename);
#endif

    return JDR_INP;
}

/***************************************************************************************
** Function name:           getJpgSize
** Description:             Get width and height of a jpg file (name in char array)
***************************************************************************************/
// Generic file call for SD or SPIFFS, uses leading / to distinguish SPIFFS files
JRESULT TJpg_Decoder::getJpgSize(uint16_t *w, uint16_t *h, const char *pFilename){

#if defined (ESP8266) || defined (ESP32)
#if defined (TJPGD_LOAD_SD_LIBRARY)
    if (*pFilename == '/')
#endif
    return getFsJpgSize(w, h, pFilename);
#endif

#if defined (TJPGD_LOAD_SD_LIBRARY)
    return getSdJpgSize(w, h, pFilename);
#endif

    return JDR_INP;
}

/***************************************************************************************
** Function name:           getJpgSize
** Description:             Get width and height of a jpg file (name in string)
***************************************************************************************/
// Generic file call for SD or SPIFFS, uses leading / to distinguish SPIFFS files
JRESULT TJpg_Decoder::getJpgSize(uint16_t *w, uint16_t *h, const string& pFilename){

#if defined (ESP8266) || defined (ESP32)
#if defined (TJPGD_LOAD_SD_LIBRARY)
    if (pFilename.charAt(0) == '/')
#endif
    return getFsJpgSize(w, h, pFilename);
#endif

#if defined (TJPGD_LOAD_SD_LIBRARY)
    return getSdJpgSize(w, h, pFilename);
#endif

    return JDR_INP;
}

#endif

#ifdef TJPGD_LOAD_SPIFFS

/***************************************************************************************
** Function name:           drawFsJpg
** Description:             Draw a named jpg SPIFFS file at x,y (name in char array)
***************************************************************************************/
// Call specific to SPIFFS
JRESULT TJpg_Decoder::drawFsJpg(int32_t x, int32_t y, const char *pFilename) {
#ifdef USE_LITTLEFS
  // Check if file exists
  if ( !LittleFS.exists(pFilename) )
  {
    Serial.println(F("Jpeg file not found"));
    return JDR_INP;
  }

    return drawFsJpg(x, y, LittleFS.open( pFilename, "r"));
#else
  // Check if file exists
  if ( !SPIFFS.exists(pFilename) )
  {
    Serial.println(F("Jpeg file not found"));
    return JDR_INP;
  }

    return drawFsJpg(x, y, SPIFFS.open( pFilename, "r"));
#endif
}

/***************************************************************************************
** Function name:           drawFsJpg
** Description:             Draw a named jpg SPIFFS file at x,y (name in string)
***************************************************************************************/
JRESULT TJpg_Decoder::drawFsJpg(int32_t x, int32_t y, const string& pFilename) {
#ifdef USE_LITTLEFS
  // Check if file exists
  if ( !LittleFS.exists(pFilename) )
  {
    Serial.println(F("Jpeg file not found"));
    return JDR_INP;
  }

    return drawFsJpg(x, y, LittleFS.open( pFilename, "r"));
#else
  // Check if file exists
  if ( !SPIFFS.exists(pFilename) )
  {
    Serial.println(F("Jpeg file not found"));
    return JDR_INP;
  }
#endif
    return drawFsJpg(x, y, SPIFFS.open( pFilename, "r"));
}

/***************************************************************************************
** Function name:           drawFsJpg
** Description:             Draw a jpg with opened SPIFFS file handle at x,y
***************************************************************************************/
JRESULT TJpg_Decoder::drawFsJpg(int32_t x, int32_t y, fs::File inFile) {
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  jpg_source = TJPG_FS_FILE;
  jpeg_x = x;
  jpeg_y = y;

  jdec.swap = _swap;

  jpgFile = inFile;

  jresult = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, (unsigned int)0);

  // Extract image and render
  if (jresult == JDR_OK) {
    jresult = jd_decomp(&jdec, jd_output, jpgScale);
  }

  // Close file
  if (jpgFile) jpgFile.close();

  return jresult;

}

/***************************************************************************************
** Function name:           getFsJpgSize
** Description:             Get width and height of a jpg saved in  SPIFFS
***************************************************************************************/
// Call specific to SPIFFS
JRESULT TJpg_Decoder::getFsJpgSize(uint16_t *w, uint16_t *h, const char *pFilename) {
#ifdef USE_LITTLEFS
  // Check if file exists
  if ( !LittleFS.exists(pFilename) )
  {
    Serial.println(F("Jpeg file not found"));
    return JDR_INP;
  }

    return getFsJpgSize(w, h, LittleFS.open( pFilename, "r"));
#else
  // Check if file exists
  if ( !SPIFFS.exists(pFilename) )
  {
    Serial.println(F("Jpeg file not found"));
    return JDR_INP;
  }

    return getFsJpgSize(w, h, SPIFFS.open( pFilename, "r"));
#endif
}

/***************************************************************************************
** Function name:           getFsJpgSize
** Description:             Get width and height of a jpg saved in  SPIFFS
***************************************************************************************/
JRESULT TJpg_Decoder::getFsJpgSize(uint16_t *w, uint16_t *h, const string& pFilename) {
#ifdef USE_LITTLEFS
  // Check if file exists
  if ( !LittleFS.exists(pFilename) )
  {
    Serial.println(F("Jpeg file not found"));
    return JDR_INP;
  }

    return getFsJpgSize(w, h, LittleFS.open( pFilename, "r"));
#else
  // Check if file exists
  if ( !SPIFFS.exists(pFilename) )
  {
    Serial.println(F("Jpeg file not found"));
    return JDR_INP;
  }

    return getFsJpgSize(w, h, SPIFFS.open( pFilename, "r"));
#endif
}

/***************************************************************************************
** Function name:           getFsJpgSize
** Description:             Get width and height of a jpg saved in SPIFFS
***************************************************************************************/
JRESULT TJpg_Decoder::getFsJpgSize(uint16_t *w, uint16_t *h, fs::File inFile) {
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  *w = 0;
  *h = 0;

  jpg_source = TJPG_FS_FILE;

  jpgFile = inFile;

  jresult = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);

  if (jresult == JDR_OK) {
    *w = jdec.width;
    *h = jdec.height;
  }

  // Close file
  if (jpgFile) jpgFile.close();

  return jresult;
}

#endif


#if defined (TJPGD_LOAD_SD_LIBRARY)

/***************************************************************************************
** Function name:           drawSdJpg
** Description:             Draw a named jpg SD file at x,y (name in char array)
***************************************************************************************/
// Call specific to SD
JRESULT TJpg_Decoder::drawSdJpg(int32_t x, int32_t y, const char *pFilename) {

  // Check if file exists
  // if ( !SD.exists(pFilename) )
  // {
  //   Serial.println(F("Jpeg file not found"));
  //   return JDR_INP;
  // }

  FRESULT fr;
  f_open(&thisPtr->jpgSdFile, pFilename, FA_READ);

  if (fr){
    return JDR_INP;
  }

    // return drawSdJpg(x, y, SD.open( pFilename, FILE_READ));
    return drawSdJpg(x, y, thisPtr->jpgSdFile);
}

/***************************************************************************************
** Function name:           drawSdJpg
** Description:             Draw a named jpg SD file at x,y (name in string)
***************************************************************************************/
JRESULT TJpg_Decoder::drawSdJpg(int32_t x, int32_t y, const string& pFilename) {

  // Check if file exists
  // if ( !SD.exists(pFilename) )
  // {
  //   Serial.println(F("Jpeg file not found"));
  //   return JDR_INP;
  // }

  FRESULT fr;
  f_open(&thisPtr->jpgSdFile, pFilename.c_str(), FA_READ);

  if (fr){
    return JDR_INP;
  }

    // return drawSdJpg(x, y, SD.open( pFilename, FILE_READ));
    return drawSdJpg(x, y, thisPtr->jpgSdFile);
}

/***************************************************************************************
** Function name:           drawSdJpg
** Description:             Draw a jpg with opened SD file handle at x,y
***************************************************************************************/
JRESULT TJpg_Decoder::drawSdJpg(int32_t x, int32_t y, FIL inFile) {
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  jpg_source = TJPG_SD_FILE;
  jpeg_x = x;
  jpeg_y = y;

  jdec.swap = _swap;

  jpgSdFile = inFile;

  jresult = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);

  // Extract image and render
  if (jresult == JDR_OK) {
    jresult = jd_decomp(&jdec, jd_output, jpgScale);
  }

  // Close file
  // if (jpgSdFile) jpgSdFile.close();
  f_close(&jpgSdFile);

  return jresult;

}


/***************************************************************************************
** Function name:           drawSdAudioJpg
** Description:             Draw a named jpg SD file at x,y (name in char array)
***************************************************************************************/
// Call specific to SD
JRESULT TJpg_Decoder::drawSdAudioJpg(int32_t x, int32_t y, const char *pFilename, int32_t jpegOffset, int32_t jpegSize) {

  // Check if file exists
  // if ( !SD.exists(pFilename) )
  // {
  //   Serial.println(F("Jpeg file not found"));
  //   return JDR_INP;
  // }

  FRESULT fr;
  f_open(&thisPtr->jpgSdFile, pFilename, FA_READ);

  if (fr){
    return JDR_INP;
  }

    // return drawSdAudioJpg(x, y, SD.open( pFilename, FILE_READ), jpegOffset, jpegSize);
    return drawSdAudioJpg(x, y, thisPtr->jpgSdFile, jpegOffset, jpegSize);
}

/***************************************************************************************
** Function name:           drawSdAudioJpg
** Description:             Draw a named jpg SD file at x,y (name in string)
***************************************************************************************/
JRESULT TJpg_Decoder::drawSdAudioJpg(int32_t x, int32_t y, const string& pFilename, int32_t jpegOffset, int32_t jpegSize) {

  // Check if file exists
  // if ( !SD.exists(pFilename) )
  // {
  //   Serial.println(F("Jpeg file not found"));
  //   return JDR_INP;
  // }
  FRESULT fr;
  f_open(&thisPtr->jpgSdFile, pFilename.c_str(), FA_READ);

  if (fr){
    return JDR_INP;
  }

    // return drawSdAudioJpg(x, y, SD.open( pFilename, FILE_READ), jpegOffset, jpegSize);
  return drawSdAudioJpg(x, y, thisPtr->jpgSdFile, jpegOffset, jpegSize);
}

/***************************************************************************************
** Function name:           drawSdAudioJpg
** Description:             Draw a jpg in audio file with opened SD file handle at x,y
***************************************************************************************/
JRESULT TJpg_Decoder::drawSdAudioJpg(int32_t x, int32_t y, FIL inFile, int32_t jpegOffset, int32_t jpegSize) {
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  jpg_source = TJPG_SD_AUDIO_FILE;
  jpeg_x = x;
  jpeg_y = y;

  jpeg_offset = jpegOffset;
  jpeg_size = jpegSize;
  
  jpeg_start = true;
  jpeg_buf_count = 0;

  jdec.swap = _swap;

  jpgSdFile = inFile;

  jresult = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);

  // Extract image and render
  if (jresult == JDR_OK) {
    jresult = jd_decomp(&jdec, jd_output, jpgScale);
  }

  // Close file
  // if (jpgSdFile) jpgSdFile.close();
  f_close(&jpgSdFile);

  return jresult;

}

/***************************************************************************************
** Function name:           getSdJpgSize
** Description:             Get width and height of a jpg saved in  SPIFFS
***************************************************************************************/
// Call specific to SD
JRESULT TJpg_Decoder::getSdJpgSize(uint16_t *w, uint16_t *h, const char *pFilename) {

  // Check if file exists
  // if ( !SD.exists(pFilename) )
  // {
  //   Serial.println(F("Jpeg file not found"));
  //   return JDR_INP;
  // }

  FRESULT fr;
  f_open(&thisPtr->jpgSdFile, pFilename, FA_READ);

  if (fr){
    return JDR_INP;
  }

    // return getSdJpgSize(w, h, SD.open( pFilename, FILE_READ));
    return getSdJpgSize(w, h, thisPtr->jpgSdFile);
}

/***************************************************************************************
** Function name:           getSdJpgSize
** Description:             Get width and height of a jpg saved in  SPIFFS
***************************************************************************************/
JRESULT TJpg_Decoder::getSdJpgSize(uint16_t *w, uint16_t *h, const string& pFilename) {

  // Check if file exists
  // if ( !SD.exists(pFilename) )
  // {
  //   Serial.println(F("Jpeg file not found"));
  //   return JDR_INP;
  // }

  FRESULT fr;
  f_open(&thisPtr->jpgSdFile, pFilename.c_str(), FA_READ);

  if (fr){
    return JDR_INP;
  }

    // return getSdJpgSize(w, h, SD.open( pFilename, FILE_READ));
    return getSdJpgSize(w, h, thisPtr->jpgSdFile);
}

/***************************************************************************************
** Function name:           getSdJpgSize
** Description:             Get width and height of a jpg saved in SPIFFS
***************************************************************************************/
JRESULT TJpg_Decoder::getSdJpgSize(uint16_t *w, uint16_t *h, FIL inFile) {
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  *w = 0;
  *h = 0;

  jpg_source = TJPG_SD_FILE;

  jpgSdFile = inFile;

  jresult = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);

  if (jresult == JDR_OK) {
    *w = jdec.width;
    *h = jdec.height;
  }

  // Close file
  // if (jpgSdFile) jpgSdFile.close();
  f_close(&jpgSdFile);

  return jresult;
}


/***************************************************************************************
** Function name:           getSdAudioJpgSize
** Description:             Get width and height of a jpg saved in  SPIFFS
***************************************************************************************/
// Call specific to SD
JRESULT TJpg_Decoder::getSdAudioJpgSize(uint16_t *w, uint16_t *h, const char *pFilename, int32_t jpegOffset, int32_t jpegSize) {

  // Check if file exists
  // if ( !SD.exists(pFilename) )
  // {
  //   Serial.println(F("Jpeg file not found"));
  //   return JDR_INP;
  // }

  FRESULT fr;
  f_open(&thisPtr->jpgSdFile, pFilename, FA_READ);

  if (fr){
    return JDR_INP;
  }
    // return getSdAudioJpgSize(w, h, SD.open( pFilename, FILE_READ), jpegOffset, jpegSize);
    return getSdAudioJpgSize(w, h, thisPtr->jpgSdFile, jpegOffset, jpegSize);
}

/***************************************************************************************
** Function name:           getSdAudioJpgSize
** Description:             Get width and height of a jpg saved in  SPIFFS
***************************************************************************************/
JRESULT TJpg_Decoder::getSdAudioJpgSize(uint16_t *w, uint16_t *h, const string& pFilename, int32_t jpegOffset, int32_t jpegSize) {

  // Check if file exists
  // if ( !SD.exists(pFilename) )
  // {
  //   Serial.println(F("Jpeg file not found"));
  //   return JDR_INP;
  // }

  FRESULT fr;
  f_open(&thisPtr->jpgSdFile, pFilename.c_str(), FA_READ);

  if (fr){
    return JDR_INP;
  }

    // return getSdAudioJpgSize(w, h, SD.open( pFilename, FILE_READ), jpegOffset, jpegSize);
    return getSdAudioJpgSize(w, h, thisPtr->jpgSdFile, jpegOffset, jpegSize);
}

/***************************************************************************************
** Function name:           getSdAudioJpgSize
** Description:             Get width and height of a jpg saved in SPIFFS
***************************************************************************************/
JRESULT TJpg_Decoder::getSdAudioJpgSize(uint16_t *w, uint16_t *h, FIL inFile, int32_t jpegOffset, int32_t jpegSize) {
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  *w = 0;
  *h = 0;

  jpeg_offset = jpegOffset;
  jpeg_size = jpegSize;
  
  jpeg_start = true;
  jpeg_buf_count = 0;


  jpg_source = TJPG_SD_AUDIO_FILE;

  jpgSdFile = inFile;

  jresult = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);

  if (jresult == JDR_OK) {
    *w = jdec.width;
    *h = jdec.height;
  }

  // Close file
  // if (jpgSdFile) jpgSdFile.close();
  f_close(&thisPtr->jpgSdFile);

  return jresult;
}

#endif

/***************************************************************************************
** Function name:           drawJpg
** Description:             Draw a jpg saved in a FLASH memory array
***************************************************************************************/
JRESULT TJpg_Decoder::drawJpg(int32_t x, int32_t y, const uint8_t jpeg_data[], uint32_t  data_size) {
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  jpg_source = TJPG_ARRAY;
  array_index = 0;
  array_data  = jpeg_data;
  array_size  = data_size;

  jpeg_x = x;
  jpeg_y = y;

  jdec.swap = _swap;

  // Analyse input data
  jresult = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);

  // Extract image and render
  if (jresult == JDR_OK) {
    jresult = jd_decomp(&jdec, jd_output, jpgScale);
  }

  return jresult;
}

/***************************************************************************************
** Function name:           getJpgSize
** Description:             Get width and height of a jpg saved in a FLASH memory array
***************************************************************************************/
JRESULT TJpg_Decoder::getJpgSize(uint16_t *w, uint16_t *h, const uint8_t jpeg_data[], uint32_t  data_size) {
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  *w = 0;
  *h = 0;

  jpg_source = TJPG_ARRAY;
  array_index = 0;
  array_data  = jpeg_data;
  array_size  = data_size;

  // Analyse input data
  jresult = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);

  if (jresult == JDR_OK) {
    *w = jdec.width;
    *h = jdec.height;
  }

  return jresult;
}
