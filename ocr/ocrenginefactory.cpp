// ocrenginefactory.cpp
#include "iocrengine.h"

#ifdef Q_OS_MACOS
#include "visionocrengine.h"
#else
#include "rapidocrengine.h"   // 由原 ocrengine.h/.cpp 改名而来
#endif

IOCREngine* createOCREngine(QObject* parent)
{
#ifdef Q_OS_MACOS
    return new VisionOcrEngine(parent);
#else
    return new RapidOcrEngine(parent);
#endif
}