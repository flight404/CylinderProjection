#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/Fbo.h"
#include "cinder/ImageIo.h"
#include "cinder/Camera.h"

using namespace ci;
using namespace ci::app;
using namespace std;

const static int FBO_SIZE		= 1920;
const static int APP_WIDTH		= 1280;
const static int APP_HEIGHT		= 720;
const static float DEGREES		= 270.0f;
const static int NUM_CAMERAS	= 3;

class CylindricalApp : public AppNative {
public:
	void resize();
	void prepareSettings( Settings *settings );
	void setup();
	void keyDown( KeyEvent event );
	void mouseDown( MouseEvent event );
	void mouseDrag( MouseEvent event );
	void mouseWheel( MouseEvent event );
	void update();
	void draw();
	void drawBox();
	void drawScene();
	void drawToPanoFbo();
	void drawCylinder( float radius, float height );
	void toggleRender();
	
	// CAMERAS
	Vec3f			mSceneEye;
	CameraPersp		mSceneCam;
	float			mCamDist;
	Vec3f			mEye, mCenter, mUp;
	CameraPersp		mCenterCam;
	
	// FBO
	Vec3f			mTargets[NUM_CAMERAS];
	gl::Fbo			mFbos[NUM_CAMERAS];
	gl::Fbo			mPanoFbo;
	
	// SHADER
	gl::GlslProg	mShader;
	
	// TEXTURE
	gl::Texture		mCheckerTex;
	
	// MOUSE
	Vec2f			mMouseDragPos;
	float			mMouseXDelta;
	float			mMousePrevX;
	
	// SET
	float			mSetTotalRadians;
	float			mSetRadius;
	float			mSetHeight;
	float			mSetAngle;
	
	bool			mShowCylinder;
	bool			mWarp;
};

void CylindricalApp::resize()
{
	mCenterCam.setPerspective( 70.0f, getWindowAspectRatio(), 1.0f, 2500.0f );
}

void CylindricalApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( APP_WIDTH, APP_HEIGHT );
}

void CylindricalApp::setup()
{
	float fov = DEGREES/NUM_CAMERAS;
	mSceneEye			= Vec3f( 0.0f, 0.0f, 0.0f );
	mSceneCam.setPerspective( fov, 1.0f, 1.0f, 3000.0f );
	mSceneCam.lookAt( Vec3f::zero(), Vec3f::zAxis(), Vec3f::yAxis() );
	
	mSetTotalRadians	= toRadians( DEGREES );
	mSetRadius			= 300.0f;
	mSetHeight			= 300.0f;
	mSetAngle			= 0.0f;
	mMousePrevX			= getWindowWidth() * 0.5f;
	
	mCamDist	= 500.0f;
	mEye		= Vec3f( 0.0f, mSetHeight * 0.5f, mCamDist );
	mCenter		= Vec3f( 0.0f, mSetHeight * 0.5f, 10.0f );
	mUp			= Vec3f::yAxis();
	mCenterCam.setPerspective( 70.0f, getWindowAspectRatio(), 1.0f, 2500.0f );
	mCenterCam.lookAt( mEye, mCenter, mUp );
	
	// SHADER
	try {
		mShader			= gl::GlslProg( loadAsset( "main.vert" ), loadAsset( "main.frag" ) );
	} catch( gl::GlslProgCompileExc e ){
		std::cout << e.what() << std::endl;
		quit();
	}
	
	gl::Texture::Format texFormat;
	texFormat.setWrap( GL_REPEAT, GL_REPEAT );
	texFormat.enableMipmapping();
	texFormat.setMinFilter( GL_LINEAR_MIPMAP_LINEAR );
	texFormat.setMagFilter( GL_LINEAR_MIPMAP_LINEAR );
	
	mCheckerTex			= gl::Texture( loadImage( loadAsset( "checker.png" ) ), texFormat );
	
	// FBOS
	float radiansDelta	= mSetTotalRadians/(float)NUM_CAMERAS;
	float camAngle		= 0.0f;
	gl::Fbo::Format format;
	format.setColorInternalFormat( GL_RGB );
	for( int i=0; i<NUM_CAMERAS; i++ ){
		mFbos[i]		= gl::Fbo( FBO_SIZE, FBO_SIZE, format );
		mTargets[i]		= Vec3f( cos( camAngle ), 0.0f, sin( camAngle ) );
		camAngle		+= radiansDelta;
	}
	mPanoFbo	= gl::Fbo( FBO_SIZE * NUM_CAMERAS, FBO_SIZE, format );
	
	mShowCylinder	= true;
	mWarp			= false;
}

void CylindricalApp::keyDown( KeyEvent event )
{
	if( event.getChar() == ' ' ){
		toggleRender();
	} else if( event.getChar() == 'w' ){
		mWarp = ! mWarp;
	}
}

void CylindricalApp::mouseDown( MouseEvent event )
{
	mMousePrevX		= event.getPos().x;
}

void CylindricalApp::mouseDrag( MouseEvent event )
{
	mMouseDragPos	= event.getPos();
	mSetAngle		+= ( mMouseDragPos.x - mMousePrevX ) * 0.004f;
	mMousePrevX		= mMouseDragPos.x;
}

void CylindricalApp::mouseWheel( MouseEvent event )
{
	mCamDist += event.getWheelIncrement() * 10.0f;
	mCamDist = constrain( mCamDist, 0.0f, 800.0f );
}

void CylindricalApp::update()
{
	mSceneCam.lookAt( mSceneEye, Vec3f::zAxis(), Vec3f::yAxis() );
	
	mEye		= Vec3f( 0.0f, mSetHeight * 0.5f + mCamDist * 0.4f, mCamDist );
	mCenter		= Vec3f( 0.0f, mSetHeight * 0.5f - mCamDist * 0.25f, -mCamDist * 0.25f - 50.0f );
	mCenterCam.lookAt( mEye, mCenter, mUp );
}

void CylindricalApp::drawScene()
{	
	Vec3f dims( 100.0f, 2.0f, 10.0f );
	
	for( int i=0; i<NUM_CAMERAS; i++ ){
		mFbos[i].bindFramebuffer();
		gl::clear( Color( 0, 0, 0 ) );
		
		mSceneCam.setCenterOfInterestPoint( mSceneEye + mTargets[i] );
		gl::setMatrices( mSceneCam );
		gl::setViewport( mFbos[0].getBounds() );
		
		gl::color( ColorA( 1.0f, 1.0f, 1.0f, 1.0f ) );
		gl::enable( GL_TEXTURE_2D );
		drawBox();
		
		mFbos[i].unbindFramebuffer();
	}
}

// draw 3 FBOs into large fbo
void CylindricalApp::drawToPanoFbo()
{
	mPanoFbo.bindFramebuffer();
	gl::clear( Color( 0, 0, 0 ) );	
	gl::color( ColorA( 1.0f, 1.0f, 1.0f, 1.0f ) );
	
	gl::setMatricesWindow( mPanoFbo.getSize() );
	gl::setViewport( mPanoFbo.getBounds() );
	gl::enable( GL_TEXTURE_2D );
	for( int i=0; i<NUM_CAMERAS; i++ ){
		mFbos[i].bindTexture();
		
		Rectf rect = Rectf( FBO_SIZE * i, 0.0f, FBO_SIZE * (i+1), FBO_SIZE );
		gl::drawSolidRect( rect );
	}
	gl::disable( GL_TEXTURE_2D );
	
	mPanoFbo.unbindFramebuffer();
}

void CylindricalApp::draw()
{
	gl::disable( GL_TEXTURE_2D );
	gl::enableAlphaBlending();
	gl::enableDepthRead();
	gl::enableDepthWrite();
	
	// draw scene into 3 FBOs
	drawScene();
	
	gl::disableDepthRead();
	gl::disableDepthWrite();
	
	// draw 3 FBOs into panoFbo
	drawToPanoFbo();
	
	gl::clear( Color( 0.1f, 0.1f, 0.1f ) );
	gl::color( Color( 1, 1, 1 ) );
	gl::setMatrices( mCenterCam );
	gl::setViewport( getWindowBounds() );
	
	gl::enable( GL_TEXTURE_2D );
	gl::enableAlphaBlending();
	gl::enableDepthRead();
	gl::enableDepthWrite();
	
	// draw panoFbo with or without warping
	if( mShowCylinder ){
		mPanoFbo.bindTexture();
		if( mWarp ){
			mShader.bind();
			mShader.uniform( "panoTex", 0 );
			mShader.uniform( "radians", mSetTotalRadians );
			mShader.uniform( "radiansOffset", (float)( M_PI_2 )/mSetTotalRadians );
			mShader.uniform( "numCameras", (float)NUM_CAMERAS );
			mShader.uniform( "invNumCamsHalf", 1.0f/( (float)NUM_CAMERAS*2.0f ) );
		}
		drawCylinder( mSetRadius, mSetHeight );
		if( mWarp ){
			mShader.unbind();
		}
		
		gl::disable( GL_TEXTURE_2D );
		gl::color( Color( 0.2f, 0.2f, 0.2f ) );
		drawCylinder( mSetRadius + 0.5f, mSetHeight );
		gl::enable( GL_TEXTURE_2D );
	} else {
		gl::setMatricesWindow( getWindowSize() );
		gl::color( Color( 1, 1, 1 ) );
		mPanoFbo.bindTexture();
		if( mWarp ){
			mShader.bind();
			mShader.uniform( "panoTex", 0 );
			mShader.uniform( "radians", mSetTotalRadians );
			mShader.uniform( "radiansOffset", (float)( M_PI_2 )/mSetTotalRadians );
			mShader.uniform( "numCameras", (float)NUM_CAMERAS );
			mShader.uniform( "invNumCamsHalf", 1.0f/( (float)NUM_CAMERAS*2.0f ) );
		}
		gl::drawSolidRect( Rectf( 0.0f, 0.0f - getWindowHeight() * 0.2f, getWindowWidth(), getWindowHeight() * 1.2f ) );
		if( mWarp ){
			mShader.unbind();
		}
	}
}

void CylindricalApp::drawCylinder( float radius, float height )
{
	int numSegs		= 72;
	gl::begin( GL_TRIANGLE_STRIP );
	for( int i=0; i<=numSegs; i++ ){
		float per	= (float)i/(float)(numSegs);
		float rads	= per * mSetTotalRadians + M_PI * 0.75f + mSetAngle;
		gl::texCoord( per, 0.25f );
		gl::vertex( Vec3f( cos( rads ) * radius, 0.0f, sin( rads ) * radius ) );
		gl::texCoord( per, 0.75f );
		gl::vertex( Vec3f( cos( rads ) * radius, height, sin( rads ) * radius ) );
	}
	gl::end();
}

// long checkerboard squared tunnel
void CylindricalApp::drawBox()
{
	float texv = 10.0f;
	float xd = 500.0f * texv;
	float yd = 500.0f;
	float zd = 500.0f;
	
	Vec3f v0 = Vec3f( -xd, -yd, -zd );
	Vec3f v1 = Vec3f(  xd, -yd, -zd );
	Vec3f v2 = Vec3f( -xd,  yd, -zd );
	Vec3f v3 = Vec3f(  xd,  yd, -zd );
	Vec3f v4 = Vec3f( -xd, -yd,  zd );
	Vec3f v5 = Vec3f(  xd, -yd,  zd );
	Vec3f v6 = Vec3f( -xd,  yd,  zd );
	Vec3f v7 = Vec3f(  xd,  yd,  zd );
	
	mCheckerTex.bind();
	gl::begin( GL_QUADS );
	
	//BACK
	gl::texCoord( 0.0f, 0.0f );		gl::vertex( v0 );
	gl::texCoord( 1.0f, 0.0f );		gl::vertex( v2 );
	gl::texCoord( 1.0f, texv );		gl::vertex( v3 );
	gl::texCoord( 0.0f, texv );		gl::vertex( v1 );
	
	//FRONT
	gl::texCoord( 0.0f, 0.0f );		gl::vertex( v6 );
	gl::texCoord( 1.0f, 0.0f );		gl::vertex( v4 );
	gl::texCoord( 1.0f, texv );		gl::vertex( v5 );
	gl::texCoord( 0.0f, texv );		gl::vertex( v7 );
	
	//RIGHT
	gl::texCoord( 0.0f, 0.0f );		gl::vertex( v0 );
	gl::texCoord( 1.0f, 0.0f );		gl::vertex( v4 );
	gl::texCoord( 1.0f, 1.0f );		gl::vertex( v6 );
	gl::texCoord( 0.0f, 1.0f );		gl::vertex( v2 );
	
	//LEFT
	gl::texCoord( 0.0f, 0.0f );		gl::vertex( v3 );
	gl::texCoord( 1.0f, 0.0f );		gl::vertex( v7 );
	gl::texCoord( 1.0f, 1.0f );		gl::vertex( v5 );
	gl::texCoord( 0.0f, 1.0f );		gl::vertex( v1 );
	
	//CEILING
	gl::texCoord( 0.0f, 0.0f );		gl::vertex( v0 );
	gl::texCoord( 1.0f, 0.0f );		gl::vertex( v4 );
	gl::texCoord( 1.0f, texv );		gl::vertex( v5 );
	gl::texCoord( 0.0f, texv );		gl::vertex( v1 );
	
	//FLOOR
	gl::texCoord( 0.0f, 0.0f );		gl::vertex( v3 );
	gl::texCoord( 1.0f, 0.0f );		gl::vertex( v7 );
	gl::texCoord( 1.0f, texv );		gl::vertex( v6 );
	gl::texCoord( 0.0f, texv );		gl::vertex( v2 );
	gl::end();
	
	gl::disable( GL_TEXTURE_2D );
	
	// four long red squared pipes
	gl::color( Color( 1.0f, 0.0f, 0.0f ) );
	gl::drawCube( Vec3f( 0.0f, -8.0f, 30.0f ), Vec3f( 1000.0f, 2.0f, 2.0f ) );
	gl::drawCube( Vec3f( 0.0f,  8.0f, 30.0f ), Vec3f( 1000.0f, 2.0f, 2.0f ) );
	gl::drawCube( Vec3f( 0.0f, -8.0f,-30.0f ), Vec3f( 1000.0f, 2.0f, 2.0f ) );
	gl::drawCube( Vec3f( 0.0f,  8.0f,-30.0f ), Vec3f( 1000.0f, 2.0f, 2.0f ) );
}

void CylindricalApp::toggleRender()
{
	mShowCylinder = !mShowCylinder;
	if( mShowCylinder ){
		setWindowSize( 1280, 720 );
	} else {
		setWindowSize( 1500, 500 );
	}
}

CINDER_APP_NATIVE( CylindricalApp, RendererGl )
