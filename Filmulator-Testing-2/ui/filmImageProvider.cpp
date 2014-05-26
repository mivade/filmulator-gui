#include "filmImageProvider.h"
#include "../database/exifFunctions.h"
#include <pwd.h>
#include <unistd.h>
#include <QTimer>
#include <cmath>
#define TIMEOUT 0.1

FilmImageProvider::FilmImageProvider() :
    QObject(0),
    QQuickImageProvider(QQuickImageProvider::Image,
                        QQuickImageProvider::ForceAsynchronousImageLoading)
{
    valid = none;

    highlights = 0;

    exposureComp = 0.0;

    develTime = 100.0;
    agitateCount = 1;
    develSteps = 12;
    filmArea = 864.0;
    layerMixConst = 0.2;

    blackpoint = 0;
    whitepoint = .002;
    //defaultToneCurveEnabled = true;

    shadowsX = 0.25;
    shadowsY = 0.25;
    highlightsX = 0.75;
    highlightsY = 0.75;

    saturation = 0;
    vibrance = 0;

    saveTiff = false;
    saveJpeg = false;

    zeroHistogram(finalHist);
    //histFinalRoll++;//signal histogram updates
    zeroHistogram(postFilmHist);
    //histPostFilmRoll++;
    zeroHistogram(preFilmHist);
    //histPreFilmRoll++;

    cout << "Constructing FilmImageProvider" << endl;
}

FilmImageProvider::~FilmImageProvider()
{
}

QImage FilmImageProvider::requestImage(const QString &id,
                                       QSize *size, const QSize &requestedSize)
{
    gettimeofday(&request_start_time,NULL);
    QImage output = emptyImage();
    QString tempID = id;
    cout << id.toStdString() << endl;
    tempID.remove(tempID.length()-6,6);
    tempID.remove(0,7);
    cout << tempID.toStdString() << endl;
    string inputFilename = tempID.toStdString();
    std::vector<std::string> inputFilenameList;
    inputFilenameList.push_back(inputFilename);
    param.filenameList = inputFilenameList;

    matrix<unsigned short> image = pipeline.processImage( param, this );

    int nrows = image.nr();
    int ncols = image.nc();

    output = QImage(ncols/3,nrows,QImage::Format_ARGB32);
    for(int i = 0; i < nrows; i++)
    {
        QRgb *line = (QRgb *)output.scanLine(i);
        for(int j = 0; j < ncols; j = j + 3)
        {
            *line = QColor(image(i,j)/256,
                           image(i,j+1)/256,
                           image(i,j+2)/256).rgb();
            line++;
        }
    }

    tout << "Request time: " << timeDiff(request_start_time) << " seconds" << endl;
    setProgress(1);
    *size = output.size();
    return output;
}

//===After load, during demosaic===

//Automatic CA correction
void FilmImageProvider::setCaEnabled(bool caEnabledIn)
{
    //QMutexLocker locker (&mutex);
    caEnabled = caEnabledIn;
    param.caEnabled = caEnabledIn;
    pipeline.abort();
    //if (valid > load)
    //    valid = load;
    emit caEnabledChanged();
}

//Highlight recovery parameter
void FilmImageProvider::setHighlights(int highlightsIn)
{
    //QMutexLocker locker (&mutex);
    highlights = highlightsIn;
    param.highlights = highlightsIn;
    pipeline.abort();
    //if (valid > load)
    //    valid = load;
    emit highlightsChanged();
}

//===After demosaic, during prefilmulation===
//Exposure compensation of simulated film exposure
void FilmImageProvider::setExposureComp(float exposure)
{
    //QMutexLocker locker(&mutex);
    exposureComp = exposure;
    std::vector<float> exposureList;
    exposureList.push_back( exposure );
    param.exposureComp = exposureList;
    pipeline.abort();
    //if (valid > demosaic)
    //    valid = demosaic;
    emit exposureCompChanged();
}

//Temperature of white balance correction
void FilmImageProvider::setTemperature(double temperatureIn)
{
    //QMutexLocker locker(&mutex);
    temperature = temperatureIn;
    param.temperature = temperatureIn;
    pipeline.abort();
    //if (valid > demosaic)
    //    valid = demosaic;
    emit temperatureChanged();
}

//Tint of white balance correction
void FilmImageProvider::setTint(double tintIn)
{
    //QMutexLocker locker(&mutex);
    tint = tintIn;
    param.tint = tintIn;
    pipeline.abort();
    //if (valid > demosaic)
    //    valid = demosaic;
    emit tintChanged();
}

//===After prefilmulation, during filmulation===
//Sets the simulated duration of film development.
void FilmImageProvider::setDevelTime(float develTimeIn)
{
    //QMutexLocker locker (&mutex);
    develTime = develTimeIn;
    param.filmParams.totalDevelTime = develTimeIn;
    pipeline.abort();
    //if (valid > prefilmulation)
    //    valid = prefilmulation;
    emit develTimeChanged();
}

//Sets the number of times the tank is agitated
void FilmImageProvider::setAgitateCount(int agitateCountIn)
{
    //QMutexLocker locker (&mutex);
    agitateCount = agitateCountIn;
    param.filmParams.agitateCount = agitateCountIn;
    pipeline.abort();
    //if (valid > prefilmulation)
    //    valid = prefilmulation;
    emit agitateCountChanged();
}

//Sets the number of simulation steps for development
void FilmImageProvider::setDevelSteps(int develStepsIn)
{
    //QMutexLocker locker (&mutex);
    develSteps = develStepsIn;
    param.filmParams.developmentSteps = develStepsIn;
    pipeline.abort();
    //if (valid > prefilmulation)
    //    valid = prefilmulation;
    emit develStepsChanged();
}

//Sets the area of the film being simulated by filmulator
void FilmImageProvider::setFilmArea(float filmAreaIn)
{
    //QMutexLocker locker (&mutex);
    filmArea = filmAreaIn;
    param.filmParams.filmArea = filmAreaIn;
    pipeline.abort();
    //if (valid > prefilmulation)
    //    valid = prefilmulation;
    emit filmAreaChanged();
}

//Sets the amount of developer mixing between the bulk
// developer reservoir and the active layer against the
// film.
void FilmImageProvider::setLayerMixConst(float layerMixConstIn)
{
    //QMutexLocker locker (&mutex);
    layerMixConst = layerMixConstIn;
    param.filmParams.layerMixConst = layerMixConstIn;
    pipeline.abort();
    //if (valid > prefilmulation)
    //    valid = prefilmulation;
    emit layerMixConstChanged();
}

//===After filmulation, during whiteblack===
//Sets the black clipping point right after filmulation
void FilmImageProvider::setBlackpoint(float blackpointIn)
{
    //QMutexLocker locker (&mutex);
    blackpoint = blackpointIn;
    param.blackpoint = blackpointIn;
    pipeline.abort();
    //if (valid > filmulation)
    //    valid = filmulation;
    emit blackpointChanged();
}

//Sets the white clipping point right after filmulation
void FilmImageProvider::setWhitepoint(float whitepointIn)
{
    //QMutexLocker locker (&mutex);
    whitepoint = whitepointIn;
    param.whitepoint = whitepointIn;
    pipeline.abort();
    //if (valid > filmulation)
    //    valid = filmulation;
    emit whitepointChanged();
}

//===After whiteblack, during colorcurve===

//===After colorcurve, during filmlikecurve===
//Y value of shadow control curve point
void FilmImageProvider::setShadowsY(float shadowsYIn)
{
    //QMutexLocker locker (&mutex);
    shadowsY = shadowsYIn;
    param.shadowsY = shadowsYIn;
    pipeline.abort();
    //if (valid > colorcurve)
    //    valid = colorcurve;
    emit shadowsYChanged();
}

//Y value of highlight control curve point
void FilmImageProvider::setHighlightsY(float highlightsYIn)
{
    //QMutexLocker locker (&mutex);
    highlightsY = highlightsYIn;
    param.highlightsY = highlightsYIn;
    pipeline.abort();
    //if (valid > colorcurve)
    //    valid = colorcurve;
    emit highlightsYChanged();
}

void FilmImageProvider::setVibrance(float vibranceIn)
{
    //QMutexLocker locker (&mutex);
    vibrance = vibranceIn;
    param.vibrance = vibranceIn;
    pipeline.abort();
    //if (valid > colorcurve)
    //    valid = colorcurve;
    emit vibranceChanged();
}

void FilmImageProvider::setSaturation(float saturationIn)
{
    //QMutexLocker locker (&mutex);
    saturation = saturationIn;
    param.saturation = saturationIn;
    pipeline.abort();
    //if (valid > colorcurve)
    //    valid = colorcurve;
    emit saturationChanged();
}

void FilmImageProvider::setProgress(float percentDone_in)
{
    progress = percentDone_in;
    emit progressChanged();
}

void FilmImageProvider::setSaveTiff(bool saveTiffIn)
{
    QMutexLocker locker (&mutex);
    saveTiff = saveTiffIn;
    emit saveTiffChanged();
}

void FilmImageProvider::setSaveJpeg(bool saveJpegIn)
{
    QMutexLocker locker (&mutex);
    saveJpeg = saveJpegIn;
    emit saveJpegChanged();
}

void FilmImageProvider::updateFilmProgress(float percentDone_in)//Percent filmulation
{
    progress = 0.2 + percentDone_in*0.6;
    emit progressChanged();
}

void FilmImageProvider::invalidateImage()
{
    cout << "filmImageProvider::invalidateImage(). We probably shouldn't be using this." << endl;
    QMutexLocker locker(&mutex);
    valid = none;
}

float FilmImageProvider::getHistogramPoint(Histogram &hist, int index, int i, LogY isLog)
{
    //index is 0 for L, 1 for R, 2 for G, and 3 for B.
    assert((index < 4) && (index >= 0));
    switch (index)
    {
    case 0: //luminance
        if (!isLog)
            return float(min(hist.lHist[i],hist.lHistMax))/float(hist.lHistMax);
        else
            return log(hist.lHist[i]+1)/log(hist.lHistMax+1);
    case 1: //red
        if (!isLog)
            return float(min(hist.rHist[i],hist.rHistMax))/float(hist.rHistMax);
        else
            return log(hist.rHist[i]+1)/log(hist.rHistMax+1);
    case 2: //green
        if (!isLog)
            return float(min(hist.gHist[i],hist.gHistMax))/float(hist.gHistMax);
        else
            return log(hist.gHist[i]+1)/log(hist.gHistMax+1);
    default://case 3: //blue
        if (!isLog)
            return float(min(hist.bHist[i],hist.bHistMax))/float(hist.bHistMax);
        else
            return log(hist.bHist[i]+1)/log(hist.bHistMax+1);
    }
    //xHistMax is the maximum height of any bin except the extremes.

    //return float(min(hist.allHist[i*4+index],hist.histMax[index]))/float(hist.histMax[index]); //maximum is the max of all elements except 0 and 127
}

QImage FilmImageProvider::emptyImage()
{
    return QImage(0,0,QImage::Format_ARGB32);
}

void FilmImageProvider::rotateLeft()
{
    rotation++;
    if (rotation > 3)
    {
        rotation -= 4;
    }
    param.rotation = rotation;
    pipeline.abort();
}

void FilmImageProvider::rotateRight()
{
    rotation--;
    if (rotation < 0)
    {
        rotation += 4;
    }
    param.rotation = rotation;
    pipeline.abort();
}
