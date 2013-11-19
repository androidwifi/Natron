//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
*Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012. 
*contact: immarespond at gmail dot com
*
*/

#include "ScaleSlider.h"

#include <cassert>
#include <qlayout.h>
#if QT_VERSION < 0x050000
CLANG_DIAG_OFF(unused-private-field);
#include <QtGui/qmime.h>
CLANG_DIAG_ON(unused-private-field);
#endif
#include <QtGui/QPaintEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>


#define TICK_HEIGHT 7
#define SLIDER_WIDTH 4
#define SLIDER_HEIGHT 20



ScaleSlider::ScaleSlider(double bottom, double top, double initialPos, Natron::Scale_Type type, QWidget* parent):
QGLWidget(parent,NULL)
, _zoomCtx()
, _textRenderer()
, _minimum(bottom)
, _maximum(top)
, _type(type)
, _position(initialPos)
, _XValues()
, _dragging(false)
, _font(new QFont("Times",8))
, _clearColor(50,50,50,255)
, _majorAxisColor(100,100,100,255)
, _scaleColor(100,100,100,255)
, _sliderColor(97,83,30,255)
, _initialized(false)
, _mustInitializeSliderPosition(true)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

QSize ScaleSlider::sizeHint() const{
    return QSize(150,30);
}

ScaleSlider::~ScaleSlider(){
    delete _font;
}

void ScaleSlider::initializeGL(){

    seekScalePosition(_position);
    _initialized = true;
}

void ScaleSlider::resizeGL(int width,int height){
    if(height == 0)
        height = 1;
    glViewport (0, 0, width , height);
}

void ScaleSlider::paintGL(){
    if(_mustInitializeSliderPosition){
        _mustInitializeSliderPosition = false;
        seekScalePosition(_position);
    }

    double w = (double)width();
    double h = (double)height();
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity();
    assert(_zoomCtx._zoomFactor > 0);
    assert(_zoomCtx._zoomFactor <= 1024);
    double bottom = _zoomCtx._bottom;
    double left = _zoomCtx._left;
    double top = bottom +  h / (double)_zoomCtx._zoomFactor;
    double right = left +  (w / (double)_zoomCtx._zoomFactor);
    if(left == right || top == bottom){
        glClearColor(_clearColor.redF(),_clearColor.greenF(),_clearColor.blueF(),_clearColor.alphaF());
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    _zoomCtx._lastOrthoLeft = left;
    _zoomCtx._lastOrthoRight = right;
    _zoomCtx._lastOrthoBottom = bottom;
    _zoomCtx._lastOrthoTop = top;
    glOrtho(left , right, bottom, top, -1, 1);
    checkGLErrors();

    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(_clearColor.redF(),_clearColor.greenF(),_clearColor.blueF(),_clearColor.alphaF());
    glClear(GL_COLOR_BUFFER_BIT);

    drawScale();
}

void ScaleSlider::drawScale(){

    //reset back the color
    glColor4f(1., 1., 1., 1.);


    QPointF btmLeft = toImgCoordinates_fast(0,height()-1);
    QPointF topRight = toImgCoordinates_fast(width()-1, 0);


    QFontMetrics fontM(*_font);

    /*drawing X axis*/
    double scaleWidth = width();
    double scaleYpos = btmLeft.y();
    double lineYpos = toImgCoordinates_fast(0,height() -1 - fontM.height()  - TICK_HEIGHT/2).y();
    double tickBottom = toImgCoordinates_fast(0,height() -1 - fontM.height() ).y();
    double tickTop = toImgCoordinates_fast(0,height() -1 - fontM.height()  - TICK_HEIGHT).y();
    double averageTextUnitWidth = 0.;

    /*draw the horizontal axis*/
    glColor4f(_scaleColor.redF(), _scaleColor.greenF(), _scaleColor.blueF(), _scaleColor.alphaF());
    glBegin(GL_LINES);
    glVertex2f(btmLeft.x(), lineYpos);
    glVertex2f(topRight.x(), lineYpos);
    glEnd();


    averageTextUnitWidth = (fontM.width(QString::number(_minimum))
                            + fontM.width(QString::number(_maximum))) / 2.;

    int majorTicksCount = (scaleWidth / averageTextUnitWidth) / 2; //divide by 2 to count as much spaces between ticks as there're ticks

    double xminp,xmaxp,dist;
    std::vector<double> acceptedDistances;
    acceptedDistances.push_back(1.);
    acceptedDistances.push_back(2.);
    acceptedDistances.push_back(5.);
    acceptedDistances.push_back(10.);
    acceptedDistances.push_back(50.);
    if(_type == Natron::LINEAR_SCALE){
        ScaleSlider::LinearScale2(btmLeft.x(), topRight.x(), majorTicksCount, acceptedDistances, &xminp, &xmaxp, &dist);
    }else if(_type == Natron::LOG_SCALE){
        ScaleSlider::LogScale1(btmLeft.x(), topRight.x(), majorTicksCount, acceptedDistances, &xminp, &xmaxp, &dist);
    }

    double value = xminp;
    double prev = value;
    for(int i = 0 ; i < majorTicksCount; ++i){
        QString text = (value - prev < 1.) ?  QString::number(value,'f',1) :  QString::number((int)value);
        renderText(value ,scaleYpos , text, _scaleColor, *_font);
        /*also draw a line*/
        glColor4f(_majorAxisColor.redF(), _majorAxisColor.greenF(), _majorAxisColor.blueF(), _majorAxisColor.alphaF());
        glBegin(GL_LINES);
        glVertex2f(value, tickBottom);
        glVertex2f(value, tickTop);
        glEnd();
        //reset back the color
        glColor4f(1., 1., 1., 1.);

        prev = value;
        value += dist;
    }

    QPointF sliderBottomLeft = toImgCoordinates_fast(_position - SLIDER_WIDTH / 2,height() -1 - fontM.height()/2);
    QPointF sliderTopRight = toImgCoordinates_fast(_position + SLIDER_WIDTH / 2,height() -1 - fontM.height()/2 - SLIDER_HEIGHT);

    /*draw the slider*/

    glColor4f(_sliderColor.redF(), _sliderColor.greenF(), _sliderColor.blueF(), _sliderColor.alphaF());
    glBegin(GL_POLYGON);
    glVertex2f(sliderBottomLeft.x(),sliderBottomLeft.y());
    glVertex2f(sliderBottomLeft.x(),sliderTopRight.y());
    glVertex2f(sliderTopRight.x(),sliderTopRight.y());
    glVertex2f(sliderTopRight.x(),sliderBottomLeft.y());
    glEnd();

    /*draw a black rect around the slider for contrast*/

    glColor4f(0.,0.,0.,1.);
    glBegin(GL_LINES);
    glVertex2f(sliderBottomLeft.x(),sliderBottomLeft.y());
    glVertex2f(sliderBottomLeft.x(),sliderTopRight.y());

    glVertex2f(sliderBottomLeft.x(),sliderTopRight.y());
    glVertex2f(sliderTopRight.x(),sliderTopRight.y());

    glVertex2f(sliderTopRight.x(),sliderTopRight.y());
    glVertex2f(sliderTopRight.x(),sliderBottomLeft.y());

    glVertex2f(sliderTopRight.x(),sliderBottomLeft.y());
    glVertex2f(sliderBottomLeft.x(),sliderBottomLeft.y());
    glEnd();

    //reset back the color
    glColor4f(1., 1., 1., 1.);
}

void ScaleSlider::renderText(double x,double y,const QString& text,const QColor& color,const QFont& font){

    if(text.isEmpty())
        return;

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity();
    double h = (double)height();
    double w = (double)width();
    /*we put the ortho proj to the widget coords, draw the elements and revert back to the old orthographic proj.*/
    glOrtho(0,w,0,h,-1,1);
    glMatrixMode(GL_MODELVIEW);
    QPointF pos = toWidgetCoordinates(x, y);
    QFontMetrics m(*_font);
    _textRenderer.renderText(pos.x() - m.width(text)/2,h-pos.y(),text,color,font);
    checkGLErrors();
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity();
    glOrtho(_zoomCtx._lastOrthoLeft,_zoomCtx._lastOrthoRight,_zoomCtx._lastOrthoBottom,_zoomCtx._lastOrthoTop,-1,1);
    glMatrixMode(GL_MODELVIEW);

}



void ScaleSlider::mousePressEvent(QMouseEvent *event){

    _zoomCtx._oldClick = event->pos();

    QGLWidget::mousePressEvent(event);
}


void ScaleSlider::mouseMoveEvent(QMouseEvent *event){
    QPoint newClick =  event->pos();
    QPointF newClick_opengl = toImgCoordinates_fast(newClick.x(),newClick.y());

    seekInternal(newClick_opengl.x());

}


void ScaleSlider::seekScalePosition(double v){
    if(v < _minimum || v > _maximum)
        return;

    _position = toWidgetCoordinates(v,0).x();
    double padding = (_maximum - _minimum) / 10.;
    double displayedRange = _maximum - _minimum + 2*padding;
    double zoomFactor = width() /displayedRange;
    zoomFactor = (zoomFactor > 0.06) ? (zoomFactor-0.05) : std::max(zoomFactor,0.01);
    assert(zoomFactor>=0.01 && zoomFactor <= 1024);
    _zoomCtx.setZoomFactor(zoomFactor);
    _zoomCtx._left = _minimum - padding ;
    _zoomCtx._bottom = 0;
    if(_initialized)
        updateGL();
}

void ScaleSlider::seekInternal(double v){

    if(v < _minimum || v > _maximum)
        return;

    _position = toWidgetCoordinates(v,0).x();
    double padding = (_maximum - _minimum) / 10.;
    double displayedRange = _maximum - _minimum + 2*padding;
    double zoomFactor = width() /displayedRange;
    zoomFactor = (zoomFactor > 0.06) ? (zoomFactor-0.05) : std::max(zoomFactor,0.01);
    assert(zoomFactor>=0.01 && zoomFactor <= 1024);
    _zoomCtx.setZoomFactor(zoomFactor);
    _zoomCtx._left = _minimum - padding ;
    _zoomCtx._bottom = 0;
    if(_initialized)
        updateGL();
    emit positionChanged(v);
}

QPointF ScaleSlider::toImgCoordinates_fast(int x,int y){
    double w = (double)width() ;
    double h = (double)height();
    double bottom = _zoomCtx._bottom;
    double left = _zoomCtx._left;
    double top =  bottom +  h / _zoomCtx._zoomFactor;
    double right = left +  w / _zoomCtx._zoomFactor;
    return QPointF((((right - left)*x)/w)+left,(((bottom - top)*y)/h)+top);
}

QPoint ScaleSlider::toWidgetCoordinates(double x, double y){
    double w = (double)width() ;
    double h = (double)height();
    double bottom = _zoomCtx._bottom;
    double left = _zoomCtx._left;
    double top =  bottom +  h / _zoomCtx._zoomFactor;
    double right = left +  w / _zoomCtx._zoomFactor;
    return QPoint((int)(((x - left)/(right - left))*w),(int)(((y - top)/(bottom - top))*h));
}

// Algorithms SCALE1, SCALE2 and SCALE3 for Determination of Scales on Computer Generated Plots
// by	C. R. Lewart
// Algorithm 463, Collected Algorithms from ACM.
// this work published in Communications of the ACM
// vol. 16, NO. 10, October, 1973, pp.639--640.
// http://www.netlib.org/toms/463
// See http://lists.gnu.org/archive/html/octave-bug-tracker/2011-09/pdfJd5VVqNUGE.pdf
/*
 * SCALE1
 *
 *  Given xmin, xmax and n, SCALE1 finds a new range xminp and
 *  xmaxp divisible into approximately n linear intervals
 *  of size dist.
 *  vint is an array of acceptable values for dist (times
 *  an integer power of 10).
 *  sqr is an array of geometric means of adjacent values
 *  of vint.  it is used as break points to determine
 *  which vint value to assign to dist.
*/
void ScaleSlider::LinearScale1(double xmin, double xmax, int n,
                               double* xminp, double* xmaxp, double *dist){
    const double vint[4] = { 1., 2., 5., 10. };
    const double sqr[3] = { 1.414214, 3.162278, 7.071068}; //sqrt(2), sqrt(10), sqrt(50)
    const int nvnt = sizeof(vint)/sizeof(double);

    // check whether proper input values were supplied.
    assert(xmax > xmin && n != 0);

    /*  del accounts for computer round-off. del should be greater than the
     *  round-off expected from division or (double) operations, and less than
     *  the minimum increment of the plotting device used by the main program
     *  divided by the plot size times the number of intervals n.
     */
    const double del =  2e-9;
    // Find approximate interval size, a.
    double a = (xmax - xmin) / (double)n;
    double al = std::log10(a);
    int nal = al;
    if (a < 1.) {
        --nal;
    }
    // a is scaled into variable named b between 1 and 10.
    double b = a / std::pow(10.,nal);
    assert(b >= 1. && b <= 10);
    // The closest permissible value for b is found.
    int i;
    for (i=0; i<3; ++i) {
        if (b<sqr[i]) {
            break;
        }
    }
    // The interval size is computed.
    *dist =  vint[i] * std::pow(10.,nal);
    double fm1 = xmin / *dist;
    int m1 = fm1;
    if (fm1 < 0.) {
        --m1;
    }
    if(std::abs(m1 + 1 - fm1) <  del) {
        ++m1;
    }
    // The new minimum and maximum limits are found.
    *xminp = *dist * m1;
    double fm2 = xmax / *dist;
    int m2 = fm2 + 1.;
    if (fm2 <  -1.) {
        --m2;
    }
    if (std::abs(fm2 + 1 - m2) < del) {
        --m2;
    }
    *xmaxp = *dist * m2;
    // Adjust limits to account for round-off if necessary.
    if(*xminp > xmin){
        *xminp = xmin;
    }
    if(*xmaxp < xmax){
        *xmaxp = xmax;
    }
}

/*
 * SCALE2
 *
 * Given xmin, xmax and n, scale2 finds a new range xminp and
 * xmaxp divisible into exactly n linear intervals of size
 * dist, where n is greater than 1.
 */
void ScaleSlider::LinearScale2(double xmin, double xmax, int n,
                               const std::vector<double> &vint,
                               double* xminp, double* xmaxp, double *dist)
{
    //const double vint[4] = {1., 2., 5., 10., 20. }; // the original algorithm only works with these values
    const int nvnt = vint.size();

    // Check whether proper input values were supplied.
    assert(xmax > xmin && n != 0);
    
    const double del =  2e-19;
    // Find approximate interval size a.
    const double a = (xmax - xmin) / n;
    double al = std::log10(a);
    int nal = al;
    if (a < 1.) {
        --nal;
    }
    // a is scaled into variable named b between 1 and 10.
    const double b = a / std::pow(10,nal);
    assert(b >= 1. && b <= 10);

    // The closest permissible value for b is found.
    int i;
    for (i = 0; i < nvnt-1; ++i) {
        if (b < vint[i]+del) {
            break;
        }
    }
    // The interval size is computed.
    int np = n + 1;
    for (int iter = 0; np > n && i < nvnt; ++i, ++iter) { // the original algorithm didn't have i < nvnt
        //assert(iter < 2); // the original algorithm makes at most two iterations
        assert(i < nvnt);
        *dist = vint[i] * std::pow(10.,nal);
        double fm1 =  xmin / *dist;
        int m1 = fm1;
        if (fm1 < 0.) {
            --m1;
        }
        if(std::abs(m1 + 1 - fm1) <  del){
            ++m1;
        }
        // The new minimum and maximum limits are found.
        *xminp = *dist * m1;
        double fm2 = xmax / *dist;
        int m2 = fm2 + 1;
        if (fm2 <  -1.) {
            --m2;
        }
        if (std::abs(fm2 + 1 - m2) < del) {
            --m2;
        }
        *xmaxp = *dist * m2;
        // Check whether a second pass is required.
        np = m2 - m1;
    }

    int nx = (n - np) / 2;
    *xminp -= nx * *dist;
    *xmaxp = *xminp + n * *dist;
    
    if (*xminp > xmin) {
        *xminp = xmin;
    }
    if (*xmaxp < xmax) {
        *xmaxp = xmax;
    }
}

/*
 * SCALE3
 *
 * Given xmin, xmax and n, where n is greater than 1, SCALE3
 * finds a new range xminp and xmaxp divisible into exactly 
 * n logarithmic intervals, where the ratio of adjacent
 * uniformly spaced scale values is dist.
 */
void ScaleSlider::LogScale1(double xmin, double xmax, int n,
                            const std::vector<double> &vint,
                            double* xminp, double* xmaxp, double *dist)
{
    //const double vint[11] = { 10., 9., 8., 7., 6., 5., 4., 3., 2., 1., .5 };
    const int nvnt = vint.size();

    assert(xmax > xmin && n > 1 && xmin > 0.);
    
    // Check whether proper input values were supplied.
    const double del =  2e-9;
    //  Values are translated from the linear into logarithmic region.
    double xminl = std::log10(xmin);
    double xmaxl = std::log10(xmax);
    // Find approximate interval size a.
    const double a = (xmaxl - xminl) / n;
    const double al = std::log10(a);
    int nal = al;
    if (a < 1.) {
        --nal;
    }
    // a is scaled into variable named b between 1 and 10.
    const double b = a / std::pow(10.,nal);
    assert(b >= 1. && b <= 10);
#if 1
    // Fred's version
    // The closest permissible value for b is found.
    int i;
    for (i = 0; i < nvnt-1; ++i) {
        if (b < 10/vint[i]+del) {
            break;
        }
    }
    // The interval size is computed.
    int np = n + 1;
    double distl;
    for (; np > n; ++i) {
        assert(i < nvnt);
        distl = std::pow(10.,nal+1) / vint[i];
        double fm1 =  xminl / distl;
        int m1 = fm1;
        if (fm1 < 0.) {
            --m1;
        }
        if (std::abs(m1 + 1 - fm1) <  del) {
            ++m1;
        }
        // The new minimum and maximum limits are found.
        *xminp = distl * m1;
        double fm2 = xmaxl / distl;
        int m2 = fm2 + 1;
        if (fm2 <  -1.) {
            --m2;
        }
        if (std::abs(fm2 + 1 - m2) < del) {
            --m2;
        }
        *xmaxp = distl = m2;
        np = m2 - m1;
        // Check whether another pass is necessary.
    }
#else
    // Alex's version
    int i = 0;
    while(b >= 10. / vint[i] + del && i < nvnt) {
        ++i;
    }
    int i_1 = nal + 1;
    int np = 0;
    double distl;
    do{
        assert(i < nvnt);
        // bug: i_1 is not updated at the second look iteration
        distl = std::pow(10,i_1) / vint[i];
        double fm1 = xminl / distl;
        int m1 = fm1;
        if(fm1 < 0.){
            --m1;
        }
        double r_1 = (double)m1 + 1. - fm1;
        if(std::abs(r_1) < del){
            ++m1;
        }
        *xminp = distl * (double)m1;
        double fm2 = xmaxl / distl;
        int m2 = fm2 + 1.;
        if (fm2 < -1.) {
            --m2;
        }
        r_1 = fm2 + 1. - (double)m2;
        if(std::abs(r_1) < del){
            --m2;
        }
        *xmaxp = distl * (double)m2;
        
        np = m2 - m1;
        ++i;
    }while(np > n);
#endif

    int nx = (n - np) / 2;
    *xminp -= nx * distl;
    *xmaxp = *xminp + n * distl;
    // Values are translated from the logarithmic into the linear region.
    *dist = std::pow(10,distl);
    *xminp = std::pow(10,*xminp);
    *xmaxp = std::pow(10,*xmaxp);
    // Adjust limits to account for round-off if necessary.
    if(*xminp > xmin){
        *xminp = xmin;
    }
    if(*xmaxp < xmax){
        *xmaxp = xmax;
    }
}
