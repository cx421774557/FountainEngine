//TODO: replace "//GLSL exp" with better solution
//TODO: provide RTT(render to texture) function
#include <fountain/ft_render.h>
#include <fountain/ft_data.h>
#include <fountain/ft_algorithm.h>
#define GLEW_STATIC
#include <GL/glew.h>
#include <FreeImage.h>
#include <map>
#include <cmath>
#include <cstdio>

using ftRender::SubImage;
using ftRender::SubImagePool;
using ftRender::Camera;
using ftRender::ShaderProgram;

static bool alive = false;

static std::map<int, int> Hash2PicID;
static std::map<int, texInfo> PicID2TexInfo;
static int curPicID = 1;

static GLfloat circle8[8 * 2];
static GLfloat circle32[32 * 2];
static GLfloat circle128[128 * 2];

static GLfloat defaultTexCoor[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

//TODO: make this transform more simple
GLuint FT2InternalFormat[FT_FORMAT_MAX] = {GL_RGBA, GL_RGB, GL_RGBA, GL_RGB,
                                           GL_RGBA, GL_RGB, GL_RGBA, GL_RGBA
                                          };
GLuint FT2Format[FT_FORMAT_MAX] = {GL_RGBA, GL_RGB, GL_RGBA, GL_BGR,
                                   GL_BGRA, GL_LUMINANCE,
                                   GL_LUMINANCE_ALPHA, GL_RGBA
                                  };

//static GLuint VertexArrayID;

static float globalR, globalG, globalB, globalA;

static ShaderProgram *currentShader = NULL;
static Camera *currentCamera = NULL;


bool GLinit()
{
	//TODO: complete the OpenGL init state checking
	GLenum err = glewInit();
	if (GLEW_OK != err) {
		std::printf("GLEW init failed!\n");
		return false;
	} else {
		std::printf("GLEW Version: %s\n", glewGetString(GLEW_VERSION));
		std::printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
	}
	if (GLEW_VERSION_2_0) {
		std::printf("GLSL Version: %s\n",
		            glGetString(GL_SHADING_LANGUAGE_VERSION));
		std::printf("Shader supported!\n");
	} else {
		std::printf("Shader unsupported -_-|||\n");
	}
	return true;
}

void initCircleData(GLfloat *v, int n)
{
	float d = 3.14159f * 2.0f / n;
	for (int i = 0; i < n; i++) {
		v[i * 2] = std::sin(d * i);
		v[i * 2 + 1] = std::cos(d * i);
	}
}

bool ftRender::init()
{
	bool state = GLinit();

	initCircleData(circle8, 8);
	initCircleData(circle32, 32);
	initCircleData(circle128, 128);

	globalR = 1.0f;
	globalG = 1.0f;
	globalB = 1.0f;
	globalA = 1.0f;

	//TODO: find out how to use VAO
	//glGenVertexArrays(1, &VertexArrayID);
	//glBindVertexArray(VertexArrayID);
	alive = state;
	return state;
}

void ftRender::close()
{
	alive = false;
}

bool ftRender::isAlive()
{
	return alive;
}

//some OpenGL functions
inline void enableTexture2D()
{
	glEnable(GL_TEXTURE_2D);
	if (currentShader != NULL)
		currentShader->setUniform("useTex", 1.0f);
}

inline void disableTexture2D()
{
	glDisable(GL_TEXTURE_2D);
	if (currentShader != NULL)
		currentShader->setUniform("useTex", 0.0f);
}

inline void bindTexture(int id)
{
	glBindTexture(GL_TEXTURE_2D, id);
	if (currentShader != NULL)
		currentShader->setUniform("tex", id);
}

inline void drawFloat2(const GLfloat *v, int n, GLuint glType)
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, v);
	glDrawArrays(glType, 0, n);
	glDisableClientState(GL_VERTEX_ARRAY);
}

void ftRender::clearColorDepthBuffer()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void ftRender::transformBegin()
{
	glPushMatrix();
}

void ftRender::transformEnd()
{
	glPopMatrix();
}

void ftRender::ftTranslate(float x, float y, float z)
{
	glTranslatef(x, y, z);
}

void ftRender::ftTranslate(ftVec2 xy, float z)
{
	ftRender::ftTranslate(xy.x, xy.y, z);
}

void ftRender::ftRotate(float xAngle, float yAngle, float zAngle)
{
	glRotatef(xAngle, 1.0f, 0.0f, 0.0f);
	glRotatef(yAngle, 0.0f, 1.0f, 0.0f);
	glRotatef(zAngle, 0.0f, 0.0f, 1.0f);
}

void ftRender::ftScale(float scale)
{
	glScalef(scale, scale, scale);
}

void ftRender::ftTransparency(float tp)
{
	glColor4f(globalR, globalG, globalB, tp);
}

void ftRender::ftColor4f(float r, float g, float b, float a)
{
	globalR = r;
	globalG = g;
	globalB = b;
	globalA = a;
	glColor4f(r, g, b, a);
}

void ftRender::ftColor3f(float r, float g, float b)
{
	globalR = r;
	globalG = g;
	globalB = b;
	glColor3f(r, g, b);
}

void ftRender::useColor(ftColor c)
{
	ftRender::ftColor4f(c.getR(), c.getG(), c.getB(), c.getAlpha());
}

ftColor ftRender::getGlobalColor()
{
	return ftColor(globalR, globalG, globalB, globalA);
}

void ftRender::openLineSmooth()
{
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void ftRender::openPointSmooth()
{
	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void ftRender::openPolygonSmooth()
{
	glEnable(GL_POLYGON_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void ftRender::setClearColor(ftColor c)
{
	glClearColor(c.getR(), c.getG(), c.getB(), 1.0f);
}

int data2Texture(unsigned char *bits, int width, int height, int dataType)
{
	GLuint gl_texID;
	glGenTextures(1, &gl_texID);
	glBindTexture(GL_TEXTURE_2D, gl_texID);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
//	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	GLenum internalFormat = FT2InternalFormat[dataType];
	GLenum format = FT2Format[dataType];
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
	             format, GL_UNSIGNED_BYTE, bits);
	return gl_texID;
}

texInfo loadTexture(const char *filename)
{
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	FIBITMAP *dib;
	BYTE *bits;
	int width, height;
	GLuint gl_texID;
	texInfo tex;
	tex.id = -1;
	tex.w = 0;
	tex.h = 0;
	fif = FreeImage_GetFileType(filename, 0);
	if (fif == FIF_UNKNOWN)
		fif = FreeImage_GetFIFFromFilename(filename);
	if (FreeImage_FIFSupportsReading(fif))
		dib = FreeImage_Load(fif, filename, 0);
	else
		return tex;

	bits = FreeImage_GetBits(dib);
	width = FreeImage_GetWidth(dib);
	height = FreeImage_GetHeight(dib);

	if (fif == FIF_PNG)
		gl_texID = data2Texture(bits, width, height, FT_BGRA);
	else
		gl_texID = data2Texture(bits, width, height, FT_BGR);

	FreeImage_Unload(dib);
	tex.id = gl_texID;
	tex.w = width;
	tex.h = height;
	return tex;
}

void ftRender::drawBitmap(unsigned char *bits, int width, int height, int dataType, int x, int y)
{
	glRasterPos2i(x, y);
	glDrawPixels(width, height, FT2Format[dataType], GL_UNSIGNED_BYTE, bits);
}

int ftRender::getPicture(unsigned char *bits, int width, int height, int dataType)
{
	texInfo texIf;
	texIf.id = data2Texture(bits, width, height, dataType);
	texIf.w = width;
	texIf.h = height;
	PicID2TexInfo[curPicID] = texIf;
	return curPicID++;
}

int ftRender::getPicture(const char *filename)
{
	texInfo texIf;
	int hash = ftAlgorithm::bkdrHash(filename);
	if (Hash2PicID[hash] == 0) {
		texIf = loadTexture(filename);
		PicID2TexInfo[curPicID] = texIf;
		Hash2PicID[hash] = curPicID;
		return curPicID++;
	} else
		return Hash2PicID[hash];
}

void ftRender::drawLine(float x1, float y1, float x2, float y2)
{
	GLfloat vtx[] = {x1, y1, x2, y2};
	drawFloat2(vtx, 2, GL_LINES);
}

void ftRender::drawLine(const ftVec2 & p1, const ftVec2 & p2)
{
	ftRender::drawLine(p1.x, p1.y, p2.x, p2.y);
}

void ftRender::drawQuad(float w, float h)
{
	float w2 = w / 2.0f;
	float h2 = h / 2.0f;
	GLfloat vtx[] = {-w2, -h2, -w2, h2, w2, h2, w2, -h2};
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	drawFloat2(vtx, 4, GL_TRIANGLE_FAN);
	glDisable(GL_BLEND);
}

void ftRender::drawRect(ftRect & rct, float angle)
{
	ftVec2 rPos = rct.getCenter();
	ftVec2 rSize = rct.getSize();
	ftRender::transformBegin();
	ftRender::ftTranslate(rPos.x, rPos.y);
	ftRender::ftRotate(0.0f, 0.0f, angle);
	ftRender::drawQuad(rSize.x, rSize.y);
	ftRender::transformEnd();
}

void ftRender::drawCircle(float radius)
{
	ftRender::transformBegin();
	ftRender::ftScale(radius);
	drawFloat2(circle32, 32, GL_TRIANGLE_FAN);
	ftRender::transformEnd();
}

void ftRender::drawCircleLine(float radius)
{
	ftRender::transformBegin();
	ftRender::ftScale(radius);
	drawFloat2(circle32, 32, GL_LINE_LOOP);
	ftRender::transformEnd();
}

void ftRender::drawShape(ftShape & shape, float angle)
{
	int type = shape.getType();
	const float *v = shape.getData();
	int n = shape.getN();
	ftRender::ftRotate(0.0f, 0.0f, angle);

	switch (type)
	{
	case FT_Circle:
		ftRender::drawCircle(shape.getR());
		break;

	case FT_Polygon:
		drawFloat2(v, n, GL_TRIANGLE_FAN);
		break;

	case FT_Line:
		drawFloat2(v, n, GL_LINE_STRIP);
		break;

	case FT_Rect:
		ftRender::drawQuad(v[0], v[1]);
		break;
	}
}

ftVec2 ftRender::getPicSize(int picID)
{
	texInfo tex = PicID2TexInfo[picID];
	return ftVec2(tex.w, tex.h);
}

void ftRender::drawPic(int picID)
{
	texInfo tex = PicID2TexInfo[picID];
	GLfloat w2 = tex.w / 2.0f;
	GLfloat h2 = tex.h / 2.0f;
	GLfloat vtx[] = {-w2, -h2, w2, -h2, w2, h2, -w2, h2};

	enableTexture2D();

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	bindTexture(tex.id);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, vtx);
	glTexCoordPointer(2, GL_FLOAT, 0, defaultTexCoor);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	disableTexture2D();
}

void ftRender::drawAlphaPic(int picID)
{
	texInfo tex = PicID2TexInfo[picID];
	GLfloat w2 = tex.w / 2.0f;
	GLfloat h2 = tex.h / 2.0f;
	GLfloat vtx[] = {-w2, -h2, w2, -h2, w2, h2, -w2, h2};

	enableTexture2D();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	bindTexture(tex.id);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, vtx);
	glTexCoordPointer(2, GL_FLOAT, 0, defaultTexCoor);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisable(GL_BLEND);

	disableTexture2D();
}

void ftRender::deletePicture(int picID)
{
	std::map<int, texInfo>::iterator it = PicID2TexInfo.find(picID);
	if (it != PicID2TexInfo.end()) {
		texInfo tex = it->second;
		GLuint texID = tex.id;
		glDeleteTextures(1, &texID);
		PicID2TexInfo.erase(it);
		std::map<int, int>::iterator mapIt;
		for (mapIt = Hash2PicID.begin(); mapIt != Hash2PicID.end(); ++mapIt) {
			if (mapIt->second == picID) {
				Hash2PicID.erase(mapIt);
				break;
			}
		}
	}
}

void ftRender::deleteAllPictures()
{
	GLuint texID;
	std::map<int, texInfo>::iterator mapIt;
	for (mapIt = PicID2TexInfo.begin(); mapIt != PicID2TexInfo.end(); ++mapIt) {
		texID = (mapIt->second).id;
		glDeleteTextures(1, &texID);
	}
	Hash2PicID.clear();
	PicID2TexInfo.clear();
}

//class ftRender::SubImage
SubImage::SubImage()
{
}

SubImage::SubImage(int picID)
{
	this->picID = picID;
	size = ftRender::getPicSize(picID);
	ftRect texRect(0, 0, 1, 1);
	texRect.getFloatVertex(texCoor);
}

SubImage::SubImage(int picID, ftRect & rect)
{
	this->picID = picID;
	ftVec2 pSize = ftRender::getPicSize(picID);

	size = rect.getSize();
	ftRect texRect = rect;
	texRect.inflate(1.0f / pSize.x, 1.0f / pSize.y);
	texRect.getFloatVertex(texCoor);
}

SubImage::SubImage(const char * picName, ftRect & rect)
{
	picID = ftRender::getPicture(picName);
	ftVec2 pSize = ftRender::getPicSize(picID);

	size = rect.getSize();
	ftRect texRect = rect;
	texRect.inflate(1.0f / pSize.x, 1.0f / pSize.y);
	texRect.getFloatVertex(texCoor);
}

SubImage::SubImage(SubImage image, ftRect & rect)
{
	picID = image.getPicID();
	ftVec2 pSize = ftRender::getPicSize(picID);

	size = rect.getSize();
	ftRect texRect = rect;
	const float *tC = image.getTexCoor();
	texRect.inflate(1.0f / pSize.x, 1.0f / pSize.y);
	texRect.move(tC[0], tC[1]);
	texRect.getFloatVertex(texCoor);
}

void SubImage::setPicID(int id)
{
	picID = id;
}

int SubImage::getPicID()
{
	return picID;
}

const ftVec2 & SubImage::getSize()
{
	return size;
}

void SubImage::setSize(const ftVec2 size)
{
	this->size = size;
}

const float * SubImage::getTexCoor()
{
	return texCoor;
}

//class ftRender::SubImagePool
std::map<int, SubImage> SubImagePool::getMapFromSip(int pid, const char *sipName)
{
	int x, y;
	int picN;
	char name[100];
	int rx, ry, rw, rh;
	int tmp;
	std::map<int, SubImage> ans;
	std::FILE *sipF = std::fopen(sipName, "r");
	if (sipF == NULL) return ans;
	tmp = std::fscanf(sipF, "%d %d", &x, &y);
	tmp = std::fscanf(sipF, "%d", &picN);
	for (int i = 0; i < picN; i++) {
		tmp = std::fscanf(sipF, "%s %d %d %d %d", name, &rw, &rh, &rx, &ry);
		if (tmp == EOF) break;
		ftRect r(rx, y - ry - rh, rw, rh);
		int hash = ftAlgorithm::bkdrHash(name);
		ans[hash] = SubImage(pid, r);
	}
	std::fclose(sipF);

	return ans;
}

SubImagePool::SubImagePool(const char * picName, const char *sipName)
{
	picID = ftRender::getPicture(picName);
	nameHash2SubImage = getMapFromSip(picID, sipName);
}

const SubImage & SubImagePool::getImage(const char *imageName)
{
	int hash = ftAlgorithm::bkdrHash(imageName);
	return nameHash2SubImage[hash];
}

//TODO: complete ftRender::getImage
SubImage ftRender::getImage(int picID)
{
	return SubImage(picID);;
}

SubImage ftRender::getImage(const char *filename)
{
	int picID = ftRender::getPicture(filename);
	return ftRender::getImage(picID);
}

//test
void ftRender::drawImage(SubImage & im)
{
	int picID = im.getPicID();
	ftVec2 size = im.getSize();
	const float *texCoor = im.getTexCoor();

	texInfo tex = PicID2TexInfo[picID];
	GLfloat w2 = size.x / 2.0f;
	GLfloat h2 = size.y / 2.0f;
	GLfloat vtx[] = {-w2, -h2, w2, -h2, w2, h2, -w2, h2};

	enableTexture2D();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	//GLSL exp
	bindTexture(tex.id);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, vtx);
	glTexCoordPointer(2, GL_FLOAT, 0, texCoor);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisable(GL_BLEND);

	disableTexture2D();
}

void ftRender::useFFP()
{
	glUseProgram(0);
	currentShader = NULL;
}

//class ftRender::Camera
Camera::Camera(float x, float y, float z)
{
	setPosition(x, y, z);
	setViewport(fountain::mainWin.w, fountain::mainWin.h);
	setScale(1.0f);
	setAngle(0.0f, 0.0f, 0.0f);
	setProjectionType(FT_PLANE);
}

void Camera::setPosition(float x, float y)
{
	this->x = x;
	this->y = y;
}

void Camera::setPosition(float x, float y, float z)
{
	this->x = x;
	this->y = y;
	if (z < FT_CAMERA_NEAR)
		z = FT_CAMERA_NEAR;
	this->z = z;
}

const ftVec2 Camera::getPosition()
{
	return ftVec2(x, y);
}

void Camera::move(float x, float y)
{
	this->x += x;
	this->y += y;
}

void Camera::setAngle(float x, float y, float z)
{
	this->xAngle = x;
	this->yAngle = y;
	this->zAngle = z;
}

void Camera::setViewport(int w, int h, int x, int y)
{
	viewport = ftRect(x, y, w, h);
	W2 = viewport.getW() / 2.0f;
	H2 = viewport.getH() / 2.0f;
	ratio = z / FT_CAMERA_NEAR;
	nearW2 = viewport.getW() / ratio / 2.0f;
	nearH2 = viewport.getH() / ratio / 2.0f;
}

void Camera::setViewport(ftRect vp)
{
	this->setViewport(vp.getW(), vp.getH(), vp.getX(), vp.getY());
}

void Camera::setScale(float scale)
{
	if (scale == 0)
		scale = 0.001;
	this->scale = scale;
}

void Camera::setProjectionType(int type)
{
	projectionType = type;
}

void Camera::update()
{
	glViewport(viewport.getX(), viewport.getY(), viewport.getW(), viewport.getH());
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (projectionType == FT_PLANE) {
		glOrtho(-W2 / scale, W2 / scale, -H2 / scale, H2 / scale,
		        -99999, 99999);
	} else if (projectionType == FT_PERSPECTIVE) {
		glFrustum(-nearW2 / scale, nearW2 / scale, -nearH2 / scale,
		          nearH2 / scale, FT_CAMERA_NEAR, FT_CAMERA_FAR);
	}
	glRotatef(xAngle, 1.0f, 0.0f, 0.0f);
	glRotatef(yAngle, 0.0f, 1.0f, 0.0f);
	glRotatef(zAngle, 0.0f, 0.0f, 1.0f);
	glTranslatef(-x, -y, -z);
	glMatrixMode(GL_MODELVIEW);
	currentCamera = this;
}

const ftVec2 Camera::mouseToWorld(const ftVec2 & mPos)
{
	float l, b, w2, h2;
	w2 = W2 / scale;
	h2 = H2 / scale;
	l = x - w2;
	b = y - h2;
	ftVec2 ans;
	ans.x = mPos.x / scale + l;
	ans.y = mPos.y / scale + b;
	return ans;
}

Camera* ftRender::getCurrentCamera()
{
	return currentCamera;
}

//functions used by ShaderProgram
GLuint compileShader(const GLchar *shaderStr, GLenum shaderType)
{
	GLint compiled;
	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, &shaderStr, NULL);
	glCompileShader(shader);
	//debug
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint length;
		GLchar *log;
		if (shaderType == GL_VERTEX_SHADER)
			std::printf("vertex");
		if (shaderType == GL_FRAGMENT_SHADER)
			std::printf("fragment");
		std::printf(" shader compile failed!!!\n");
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		log = new GLchar[length];
		glGetShaderInfoLog(shader, length, &length, log);
		std::printf("//\n%s//\n\n", log);
		delete [] log;
		glDeleteShader(shader);
		return 0;
	}
	//debug end
	return shader;
}

GLuint linkShaderProgram(GLuint vs, GLuint fs)
{
	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);
	glLinkProgram(program);
	return program;
}

//class ftRender::ShaderProgram
ShaderProgram::ShaderProgram()
{
	program = 0;
	vs = 0;
	fs = 0;
}

ShaderProgram::ShaderProgram(const char *vsf, const char *fsf)
{
	program = 0;
	vs = 0;
	fs = 0;
	load(vsf, fsf);
}

ShaderProgram::~ShaderProgram()
{
	free();
}

void ShaderProgram::load(const char *vsf, const char *fsf)
{
	vsFile.load(vsf);
	fsFile.load(fsf);
}

//TODO: add words to check the process
bool ShaderProgram::init()
{
	//compile
	const char *vss = vsFile.getStr();
	vs = compileShader(vss, GL_VERTEX_SHADER);
	const char *fss = fsFile.getStr();
	fs = compileShader(fss, GL_FRAGMENT_SHADER);

	//link
	program = linkShaderProgram(vs, fs);

	vsFile.free();
	fsFile.free();
	return true;
}

bool ShaderProgram::reload()
{
	bool res;
	free();
	vsFile.reload();
	fsFile.reload();
	res = init();
	use();
	return res;
}

void ShaderProgram::use()
{
	glUseProgram(program);
	currentShader = this;
	update();
}

void ShaderProgram::update()
{
}

void ShaderProgram::setUniform(const char *varName, float value)
{
	if (currentShader != this) return;
	GLint loc = glGetUniformLocation(program, varName);
	GLfloat v = value;
	if (loc != -1) {
		glUniform1f(loc, v);
	}
}

void ShaderProgram::setUniform(const char *varName, const ftVec2 & value)
{
	if (currentShader != this) return;
	const GLfloat v[] = {value.x, value.y};
	GLint loc = glGetUniformLocation(program, varName);
	if (loc != -1) {
		glUniform2fv(loc, 1, v);
	}
}

void ShaderProgram::free()
{
	if (currentShader == this) {
		ftRender::useFFP();
	}
	glDeleteProgram(program);
	vsFile.free();
	fsFile.free();
}
