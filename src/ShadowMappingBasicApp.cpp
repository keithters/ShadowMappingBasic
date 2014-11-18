/**

Keith Butters - 2014 - http://www.keithbutters.com

This app is a very basic implementation of shadow mapping in Cinder.

There are more advanced techniques to shadowmapping that result in
softer shadows, and light falloff, etc., some of which can be seen
in Eric Renaud-Houde's shadowmapping sample available in the
Cinder distribution.

Credit due to:
Cinder				- http://www.libcinder.org
Eric Renaud-Houde	- http://num3ric.com/
OpenGL SuperBible	- http://www.openglsuperbible.com/

**/

#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Batch.h"
#include "cinder/Camera.h"
#include "cinder/gl/Shader.h"
#include "cinder/gl/Fbo.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class ShadowMappingBasic : public AppNative {
  public:
	void prepareSettings( Settings *settings ) override;
	void setup() override;
	void keyDown( KeyEvent event ) override;
	void update() override;
	void draw() override;
	
	void drawScene( bool shadowMap );
	void renderDepthFbo();
	
  private:
	gl::FboRef			mFbo;
	static const int	FBO_WIDTH = 2048, FBO_HEIGHT = 2048;
	
	CameraPersp			mCam;
	CameraPersp			mLightCam;
	vec3				mLightPos;
	
	gl::GlslProgRef		mGlsl;
	gl::Texture2dRef	mShadowMapTex;
	
	gl::BatchRef		mTeapotBatch;
	gl::BatchRef		mTeapotShadowedBatch;
	gl::BatchRef		mFloorBatch;
	gl::BatchRef		mFloorShadowedBatch;
	
	float				mTime;
};

void ShadowMappingBasic::prepareSettings( Settings *settings )
{
	settings->enableHighDensityDisplay();
	settings->setWindowSize( 1024, 768 );
}

void ShadowMappingBasic::setup()
{
	mLightPos = vec3( 0.0f, 5.0f, 1.0f );
	
	gl::Texture2d::Format depthFormat;
	
#if defined( CINDER_GL_ES )
	depthFormat.setInternalFormat( GL_DEPTH_COMPONENT16 );
#else
	depthFormat.setInternalFormat( GL_DEPTH_COMPONENT32F );
	depthFormat.setCompareMode( GL_COMPARE_REF_TO_TEXTURE );
#endif
	depthFormat.setMagFilter( GL_LINEAR );
	depthFormat.setMinFilter( GL_LINEAR );
	depthFormat.setWrap( GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE );
	depthFormat.setCompareFunc( GL_LEQUAL );
	
	mShadowMapTex = gl::Texture2d::create( FBO_WIDTH, FBO_HEIGHT, depthFormat );
		
	gl::Fbo::Format fboFormat;
	fboFormat.attachment( GL_DEPTH_ATTACHMENT, mShadowMapTex );
	mFbo = gl::Fbo::create( FBO_WIDTH, FBO_HEIGHT, fboFormat );
	
	// Set up camera from the light's viewpoint
	mLightCam.setPerspective( 100.0f, mFbo->getAspectRatio(), 0.5f, 7.0f );
	mLightCam.lookAt( mLightPos, vec3( 0.0f ) );
	
	try {
#if defined( CINDER_GL_ES )
		mGlsl = gl::GlslProg::create( loadAsset( "shadow_shader_es2.vert" ), loadAsset( "shadow_shader_es2.frag" ) );
#else
		mGlsl = gl::GlslProg::create( loadAsset( "shadow_shader.vert" ), loadAsset( "shadow_shader.frag" ) );
#endif
	}
	catch ( std::exception& e ) {
		console() << e.what() << endl;
		quit();
	}
	
	auto teapot				= geom::Teapot().subdivisions( 8 );
	mTeapotBatch			= gl::Batch::create( teapot, gl::getStockShader( gl::ShaderDef() ) );
	mTeapotShadowedBatch	= gl::Batch::create( teapot, mGlsl );
	
	auto floor				= geom::Cube().size( 10.0f, 0.5f, 10.0f );
	mFloorBatch				= gl::Batch::create( floor, gl::getStockShader( gl::ShaderDef() ) );
	mFloorShadowedBatch		= gl::Batch::create( floor, mGlsl );
	
	gl::enableDepthRead();
	gl::enableDepthWrite();
}

void ShadowMappingBasic::update()
{
	// Store time so each render pass uses the same value
	mTime = getElapsedSeconds();
	mCam.setPerspective( 60.0f, getWindowAspectRatio(), 0.5f, 500.0f );
	mCam.lookAt( vec3( sin( mTime ) * 5.0f, sin( mTime ) * 2.5f + 2, 5.0f ), vec3( 0.0f ) );
}

void ShadowMappingBasic::renderDepthFbo()
{
	// Set polygon offset to battle shadow acne
	gl::enable( GL_POLYGON_OFFSET_FILL );
	glPolygonOffset( 2.0f, 2.0f );

	// Render scene to fbo from the view of the light
	gl::ScopedFramebuffer fbo( mFbo );
	gl::viewport( vec2( 0.0f ), mFbo->getSize() );
	gl::clear( Color::black() );
	gl::color( Color::white() );
	gl::setMatrices( mLightCam );

	drawScene( true );
	
	// Disable polygon offset for final render
	gl::disable( GL_POLYGON_OFFSET_FILL );
}

void ShadowMappingBasic::drawScene( bool shadowMap )
{
	gl::pushModelMatrix();
	gl::color( Color( 0.4f, 0.6f, 0.9f ) );
	gl::rotate( mTime * 2.0f, 1.0f, 1.0f, 1.0f );

	shadowMap ? mTeapotBatch->draw() : mTeapotShadowedBatch->draw();

	gl::popModelMatrix();
	
	gl::color( Color( 0.7f, 0.7f, 0.7f ) );
	gl::translate( 0.0f, -2.0f, 0.0f );
	
	shadowMap ? mFloorBatch->draw() : mFloorShadowedBatch->draw();
}

void ShadowMappingBasic::draw()
{
	renderDepthFbo();

	gl::clear( Color::black() );
	gl::setMatrices( mCam );
	gl::viewport( toPixels( getWindowSize() ) );
	
	gl::ScopedTextureBind tex( mShadowMapTex, (uint8_t) 0 );
	vec3 mvLightPos		= vec3( gl::getModelView() * vec4( mLightPos, 1.0f ) ) ;
	mat4 shadowMatrix	= mLightCam.getProjectionMatrix() * mLightCam.getViewMatrix();

	mGlsl->uniform( "uShadowMap", 0 );
	mGlsl->uniform( "uLightPos", mvLightPos );
	mGlsl->uniform( "uShadowMatrix", shadowMatrix );
	
	drawScene( false );
}

void ShadowMappingBasic::keyDown( KeyEvent event )
{
	if ( event.getCode() == KeyEvent::KEY_ESCAPE ) quit();
	if ( event.getCode() == KeyEvent::KEY_f ) setFullScreen( ! isFullScreen() );
}

CINDER_APP_NATIVE( ShadowMappingBasic, RendererGl )
