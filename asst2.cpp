////////////////////////////////////////////////////////////////////////
//
//   Harvard University
//   CS175 : Computer Graphics
//   Professor Steven Gortler
//
//   Solution by Tom Kelliher
////////////////////////////////////////////////////////////////////////

#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#if __GNUG__
#   include <tr1/memory>
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#   include <GLUT/glut.h>
#else
#   include <GL/glut.h>
#endif

#include "cvec.h"
#include "matrix4.h"
#include "geometrymaker.h"
#include "ppm.h"
#include "glsupport.h"
#include <assert.h>

using namespace std;      // for string, vector, iostream, and other standard C++ stuff
using namespace tr1; // for shared_ptr

// G L O B A L S ///////////////////////////////////////////////////

static const int G_SKY_CAM = 2;
static const int G_CUBE1 = 0;
static const int G_CUBE2 = 1;

static const int g_numObjects = 2;

static const float g_frustMinFov = 60.0;  // A minimal of 60 degree field of view
static float g_frustFovY = g_frustMinFov; // FOV in y direction (updated by updateFrustFovY)

static const float g_frustNear = -0.1;    // near plane
static const float g_frustFar = -50.0;    // far plane
static const float g_groundY = -2.0;      // y coordinate of the ground
static const float g_groundSize = 10.0;   // half the ground length

static int g_windowWidth = 512;
static int g_windowHeight = 512;
static bool g_mouseClickDown = false;    // is the mouse button pressed
static bool g_mouseLClickButton, g_mouseRClickButton, g_mouseMClickButton;
static int g_mouseClickX, g_mouseClickY; // coordinates for mouse click event
static int g_activeShader = 0;
static int g_activeView = G_SKY_CAM;
static int g_activeObject = G_CUBE1;

static bool g_skySkyFrame = false; // Sky-sky frame is active when modifying the sky camera

struct ShaderState {
  GlProgram program;

  // Handles to uniform variables
  GLint h_uLight, h_uLight2;
  GLint h_uProjMatrix;
  GLint h_uModelViewMatrix;
  GLint h_uNormalMatrix;
  GLint h_uColor;
  GLint h_uTexUnit;

  // Handles to vertex attributes
  GLint h_aPosition;
  GLint h_aNormal;
  GLint h_aTexCoord;

  ShaderState(const char* vsfn, const char* fsfn) {
    readAndCompileShader(program, vsfn, fsfn);

    const GLuint h = program; // short hand

    // Retrieve handles to uniform variables
    h_uLight = safe_glGetUniformLocation(h, "uLight");
    h_uLight2 = safe_glGetUniformLocation(h, "uLight2");
    h_uProjMatrix = safe_glGetUniformLocation(h, "uProjMatrix");
    h_uModelViewMatrix = safe_glGetUniformLocation(h, "uModelViewMatrix");
    h_uNormalMatrix = safe_glGetUniformLocation(h, "uNormalMatrix");
    h_uColor = safe_glGetUniformLocation(h, "uColor");
    h_uTexUnit = safe_glGetUniformLocation(h, "uTexUnit");

    // Retrieve handles to vertex attributes
    h_aPosition = safe_glGetAttribLocation(h, "aPosition");
    h_aNormal = safe_glGetAttribLocation(h, "aNormal");
    h_aTexCoord = safe_glGetAttribLocation(h, "aTexCoord");

    checkGlErrors();
  }

};

static const int g_numShaders = 4;
static const char * const g_shaderFilesGl2[g_numShaders][4] = {
   {"./shaders/basic-gl2.vshader", "./shaders/solid-gl2.fshader"},
  {"./shaders/basic-gl2.vshader", "./shaders/diffuse-gl2.fshader"},
  {"./shaders/basic-gl2.vshader", "./shaders/solid-gl3.fshader"},
  {"./shaders/basic-gl2.vshader", "./shaders/solid-gl4.fshader"}
};
static vector<shared_ptr<ShaderState> > g_shaderStates; // our global shader states

// --------- Geometry

// Macro used to obtain relative offset of a field within a struct
#define FIELD_OFFSET(StructType, field) &(((StructType *)0)->field)

// A vertex with floating point position and normal
struct VertexPNT {
  Cvec3f p, n;
  Cvec2f t;

  VertexPNT() {}
  VertexPNT(float x, float y, float z,
            float nx, float ny, float nz,
            float tu, float tv)
     : p(x,y,z), n(nx, ny, nz), t(tu, tv)
  {}

  // Define copy constructor and assignment operator from GenericVertex so we can
  // use make* functions from geometrymaker.h
  VertexPNT(const GenericVertex& v) {
    *this = v;
  }

  VertexPNT& operator = (const GenericVertex& v) {
    p = v.pos;
    n = v.normal;
    t = v.tex;
    return *this;
  }
};

struct Geometry {
  GlBufferObject vbo, ibo;
  int vboLen, iboLen;

  Geometry(VertexPNT *vtx, unsigned short *idx, int vboLen, int iboLen) {
    this->vboLen = vboLen;
    this->iboLen = iboLen;

    // Now create the VBO and IBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPNT) * vboLen, vtx, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * iboLen, idx, GL_STATIC_DRAW);
  }

  void draw(const ShaderState& curSS) {
    // Enable the attributes used by our shader
    safe_glEnableVertexAttribArray(curSS.h_aPosition);
    safe_glEnableVertexAttribArray(curSS.h_aNormal);
    safe_glEnableVertexAttribArray(curSS.h_aTexCoord);

    // bind vbo
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    safe_glVertexAttribPointer(curSS.h_aPosition, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNT), FIELD_OFFSET(VertexPNT, p));
    safe_glVertexAttribPointer(curSS.h_aNormal, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNT), FIELD_OFFSET(VertexPNT, n));
    safe_glVertexAttribPointer(curSS.h_aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPNT), FIELD_OFFSET(VertexPNT, t));

    // bind ibo
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

    // draw!
    glDrawElements(GL_TRIANGLES, iboLen, GL_UNSIGNED_SHORT, 0);

    // Disable the attributes used by our shader
    safe_glDisableVertexAttribArray(curSS.h_aPosition);
    safe_glDisableVertexAttribArray(curSS.h_aNormal);
    safe_glDisableVertexAttribArray(curSS.h_aTexCoord);
  }
};


// Vertex buffer and index buffer associated with the ground and cubes geometry
static shared_ptr<Geometry> g_ground, g_cube;
static shared_ptr<GlTexture> g_checkerboard;
// --------- Scene

static const Cvec3 g_light1(2.0, 3.0, 14.0), g_light2(-2, -3.0, -5.0);  // define two lights positions in world space
static Matrix4 g_worldRbt = Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0));
static Matrix4 g_skyRbt = Matrix4::makeTranslation(Cvec3(0.0, 0.25, 4.0));
static Matrix4 g_objectRbt[g_numObjects] = {
   Matrix4::makeTranslation(Cvec3(-0.75,0,0)),   //obj1 @ index 0
   Matrix4::makeTranslation(Cvec3(0.75,0,0))     // obj2 @ index 1
};
static Cvec3f g_objectColors[g_numObjects] = {
   Cvec3f(1, 0, 0),  // same deal as for g_objectRbt
   Cvec3f(0, 0, 1)
};

///////////////// END OF G L O B A L S //////////////////////////////////////////////////




static void initGround() {
  // A x-z plane at y = g_groundY of dimension [-g_groundSize, g_groundSize]^2
  VertexPNT vtx[4] = {
     VertexPNT(-g_groundSize, g_groundY, -g_groundSize, 0, 1, 0, 0., 1.),
     VertexPNT(-g_groundSize, g_groundY,  g_groundSize, 0, 1, 0, 0., 0.),
     VertexPNT( g_groundSize, g_groundY,  g_groundSize, 0, 1, 0, 1., 0.),
     VertexPNT( g_groundSize, g_groundY, -g_groundSize, 0, 1, 0, 1., 1.),
  };
  unsigned short idx[] = {0, 1, 2, 0, 2, 3};
  g_ground.reset(new Geometry(&vtx[0], &idx[0], 4, 6));
}

static void initCubes() {
  int ibLen, vbLen;
  getCubeVbIbLen(vbLen, ibLen);

  // Temporary storage for cube geometry
  vector<VertexPNT> vtx(vbLen);
  vector<unsigned short> idx(ibLen);

  makeCube(1, vtx.begin(), idx.begin());
  g_cube.reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen));
}

// takes a projection matrix and send to the the shaders
static void sendProjectionMatrix(const ShaderState& curSS, const Matrix4& projMatrix) {
  GLfloat glmatrix[16];
  projMatrix.writeToColumnMajorMatrix(glmatrix); // send projection matrix
  safe_glUniformMatrix4fv(curSS.h_uProjMatrix, glmatrix);
}

// takes MVM and its normal matrix to the shaders
static void sendModelViewNormalMatrix(const ShaderState& curSS, const Matrix4& MVM, const Matrix4& NMVM) {
  GLfloat glmatrix[16];
  MVM.writeToColumnMajorMatrix(glmatrix); // send MVM
  safe_glUniformMatrix4fv(curSS.h_uModelViewMatrix, glmatrix);

  NMVM.writeToColumnMajorMatrix(glmatrix); // send NMVM
  safe_glUniformMatrix4fv(curSS.h_uNormalMatrix, glmatrix);
}

// update g_frustFovY from g_frustMinFov, g_windowWidth, and g_windowHeight
static void updateFrustFovY() {
  if (g_windowWidth >= g_windowHeight)
    g_frustFovY = g_frustMinFov;
  else {
    const double RAD_PER_DEG = 0.5 * CS175_PI/180;
    g_frustFovY = atan2(sin(g_frustMinFov * RAD_PER_DEG) * g_windowHeight / g_windowWidth, cos(g_frustMinFov * RAD_PER_DEG)) / RAD_PER_DEG;
  }
}

static Matrix4 makeProjectionMatrix() {
  return Matrix4::makeProjection(
           g_frustFovY, g_windowWidth / static_cast <double> (g_windowHeight),
           g_frustNear, g_frustFar);
}

inline Matrix4 getEyeRbt()
{
  switch (g_activeView) {
  case G_SKY_CAM:
     return g_skyRbt;
     break;
  case G_CUBE1:
     return g_objectRbt[G_CUBE1];
     break;
  case G_CUBE2:
     return g_objectRbt[G_CUBE2];
     break;
  }
  // Execution should never reach here.  Terminate if it does.
  assert(0);
  // Dummy return value to keep the compiler happy.
  return Matrix4(0);
}

static void drawStuff() {
  // short hand for current shader state
  const ShaderState& curSS = *g_shaderStates[g_activeShader];

  // build & send proj. matrix to vshader
  const Matrix4 projmat = makeProjectionMatrix();
  sendProjectionMatrix(curSS, projmat);

  // use the skyRbt as the eyeRbt
  const Matrix4 eyeRbt = getEyeRbt();
  const Matrix4 invEyeRbt = inv(eyeRbt);

  const Cvec3 eyeLight1 = Cvec3(invEyeRbt * Cvec4(g_light1, 1)); // g_light1 position in eye coordinates
  const Cvec3 eyeLight2 = Cvec3(invEyeRbt * Cvec4(g_light2, 1)); // g_light2 position in eye coordinates
  safe_glUniform1i(curSS.h_uTexUnit,0);
  safe_glUniform3f(curSS.h_uLight, eyeLight1[0], eyeLight1[1], eyeLight1[2]);
  safe_glUniform3f(curSS.h_uLight2, eyeLight2[0], eyeLight2[1], eyeLight2[2]);

  // draw ground
  // ===========
  //
  const Matrix4 groundRbt = Matrix4();  // identity
  Matrix4 MVM = invEyeRbt * groundRbt;
  Matrix4 NMVM = normalMatrix(MVM);
  sendModelViewNormalMatrix(curSS, MVM, NMVM);
  safe_glUniform3f(curSS.h_uColor, 0.1, 0.95, 0.1); // set color
  g_ground->draw(curSS);

  // draw cubes
  // ==========
  MVM = invEyeRbt * g_objectRbt[G_CUBE1];
  NMVM = normalMatrix(MVM);
  sendModelViewNormalMatrix(curSS, MVM, NMVM);
  safe_glUniform3f(curSS.h_uColor, g_objectColors[G_CUBE1][0], g_objectColors[G_CUBE1][1], g_objectColors[G_CUBE1][2]);
  g_cube->draw(curSS);

  MVM = invEyeRbt * g_objectRbt[G_CUBE2];
  NMVM = normalMatrix(MVM);
  sendModelViewNormalMatrix(curSS, MVM, NMVM);
  safe_glUniform3f(curSS.h_uColor, g_objectColors[G_CUBE2][0], g_objectColors[G_CUBE2][1], g_objectColors[G_CUBE2][2]);
  g_cube->draw(curSS);
}

static void display() {
  glUseProgram(g_shaderStates[g_activeShader]->program);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);                   // clear framebuffer color&depth

  drawStuff();

  glutSwapBuffers();                                    // show the back buffer (where we rendered stuff)

  checkGlErrors();
}
static void loadTexture(GLuint texHandle, const char *ppmFilename) {
  int texWidth, texHeight;
  vector<PackedPixel> pixData;

  ppmRead(ppmFilename, texWidth, texHeight, pixData);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texHandle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight,
               0, GL_RGB, GL_UNSIGNED_BYTE, &pixData[0]);
  checkGlErrors();
}
static void initTextures() {
  g_checkerboard.reset(new GlTexture());

  loadTexture(*g_checkerboard, "checkerboard.ppm");


  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, *g_checkerboard);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);


}

static void reshape(const int w, const int h) {
  g_windowWidth = w;
  g_windowHeight = h;
  glViewport(0, 0, w, h);
  cerr << "Size of window is now " << w << "x" << h << endl;
  updateFrustFovY();
  glutPostRedisplay();
}

static void motion(const int x, const int y) {
  double dx = x - g_mouseClickX;
  double dy = g_windowHeight - y - 1 - g_mouseClickY;

  Matrix4 m;
  if (g_mouseLClickButton && !g_mouseRClickButton) { // left button down?
     if (g_activeObject == G_SKY_CAM || g_activeView == g_activeObject)
     {
        dx = -dx;
        dy = -dy;
     }
    m = Matrix4::makeXRotation(-dy) * Matrix4::makeYRotation(dx);
  }
  else if (g_mouseRClickButton && !g_mouseLClickButton) { // right button down?
     if (g_activeObject == G_SKY_CAM && !g_skySkyFrame)
     {
        dx = -dx;
        dy = -dy;
     }
    m = Matrix4::makeTranslation(Cvec3(dx, dy, 0) * 0.01);
  }
  else if (g_mouseMClickButton || (g_mouseLClickButton && g_mouseRClickButton)) {  // middle or (left and right) button down?
     if (g_activeObject == G_SKY_CAM && !g_skySkyFrame)
     {
        dy = -dy;
     }
    m = Matrix4::makeTranslation(Cvec3(0, 0, -dy) * 0.01);
  }

  if (g_mouseClickDown) {
    int ao = g_activeObject;  // shorthand
    if (ao == G_CUBE1 || ao == G_CUBE2)  // modifying an object
       g_objectRbt[ao] = doMtoOwrtA(m, g_objectRbt[ao],
                                makeMixedFrame(g_objectRbt[ao], getEyeRbt()));
    else  // the sky cam is the active object
       if (g_activeView == G_SKY_CAM) // modify only if the sky cam is the active view
          g_skyRbt = doMtoOwrtA(m, g_skyRbt,
                     makeMixedFrame(g_skySkyFrame ? g_skyRbt : g_worldRbt,
                                    g_skyRbt));

    glutPostRedisplay(); // we always redraw if we changed the scene
  }

  g_mouseClickX = x;
  g_mouseClickY = g_windowHeight - y - 1;
}


static void mouse(const int button, const int state, const int x, const int y) {
  g_mouseClickX = x;
  g_mouseClickY = g_windowHeight - y - 1;  // conversion from GLUT window-coordinate-system to OpenGL window-coordinate-system

  g_mouseLClickButton |= (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN);
  g_mouseRClickButton |= (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN);
  g_mouseMClickButton |= (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN);

  g_mouseLClickButton &= !(button == GLUT_LEFT_BUTTON && state == GLUT_UP);
  g_mouseRClickButton &= !(button == GLUT_RIGHT_BUTTON && state == GLUT_UP);
  g_mouseMClickButton &= !(button == GLUT_MIDDLE_BUTTON && state == GLUT_UP);

  g_mouseClickDown = g_mouseLClickButton || g_mouseRClickButton || g_mouseMClickButton;
}


static void keyboard(const unsigned char key, const int x, const int y) {
  switch (key) {
  case 27:
    exit(0);                                  // ESC
  case 'h':
    cout << " ============== H E L P ==============\n\n"
    << "h\t\thelp menu\n"
    << "s\t\tsave screenshot\n"
    << "f\t\tToggle flat shading on/off.\n"
    << "o\t\tCycle object to edit\n"
    << "v\t\tCycle view\n"
    << "drag left mouse to rotate\n" << endl;
    break;
  case 's':
    glFlush();
    writePpmScreenshot(g_windowWidth, g_windowHeight, "out.ppm");
    break;
  case 'f':
     g_activeShader = (g_activeShader + 1) % g_numShaders;

    break;
  case 'v':  // Cycle through the views, one per object, including the sky cam
    g_activeView = (g_activeView + 1) % (g_numObjects + 1);

    switch (g_activeView) {
    case G_SKY_CAM:
       cout << "Active view set to sky camera" << endl;
       break;
    case G_CUBE1:
       cout << "Active view set to cube 1" << endl;
       break;
    case G_CUBE2:
       cout << "Active view set to cube 2" << endl;
       break;
    }
    break;
  case 'o':  // Cycle through the objects, including the sky cam
    g_activeObject = (g_activeObject + 1) % (g_numObjects + 1);
    switch (g_activeObject) {
    case G_SKY_CAM:
       cout << "Active object set to sky camera" << endl;
       break;
    case G_CUBE1:
       cout << "Active object set to cube 1" << endl;
       break;
    case G_CUBE2:
       cout << "Active object set to cube 2" << endl;
       break;
    }
    break;
  case 'm':
     if (g_activeObject == G_SKY_CAM && g_activeView == G_SKY_CAM)
        g_skySkyFrame ^= true;

     switch (g_skySkyFrame) {
     case false:
        cout << "Active sky camera frame is world-sky frame" << endl;
        break;
     case true:
        cout << "Active sky camera frame is sky-sky frame" << endl;
        break;
     }
  }

  glutPostRedisplay();
}

static void initGlutState(int argc, char * argv[]) {
  glutInit(&argc, argv);                                  // initialize Glut based on cmd-line args
  glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);  //  RGBA pixel channels and double buffering
  glutInitWindowSize(g_windowWidth, g_windowHeight);      // create a window
  glutCreateWindow("Fragment Shader Fun");                // title the window

  glutDisplayFunc(display);                               // display rendering callback
  glutReshapeFunc(reshape);                               // window reshape callback
  glutMotionFunc(motion);                                 // mouse movement callback
  glutMouseFunc(mouse);                                   // mouse click callback
  glutKeyboardFunc(keyboard);
}

static void initGLState() {
  glClearColor(128./255., 200./255., 255./255., 0.);
  glClearDepth(0.);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glCullFace(GL_BACK);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GREATER);
  glReadBuffer(GL_BACK);
}

static void initShaders() {
  g_shaderStates.resize(g_numShaders);
  for (int i = 0; i < g_numShaders; ++i)
     g_shaderStates[i].reset(new ShaderState(g_shaderFilesGl2[i][0], g_shaderFilesGl2[i][1]));
}

static void initGeometry() {
  initGround();
  initCubes();
}

int main(int argc, char * argv[]) {
  try {
    initGlutState(argc,argv);

    glewInit(); // load the OpenGL extensions

    initGLState();
    initShaders();
    initGeometry();
    initTextures();
    glutMainLoop();
    return 0;
  }
  catch (const runtime_error& e) {
    cout << "Exception caught: " << e.what() << endl;
    return -1;
  }
}
