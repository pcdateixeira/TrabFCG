// Headers abaixo são específicos de C++
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>   // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h>  // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>

#include <stb_image.h>

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"

// Estrutura que representa um modelo geométrico carregado a partir de um
// arquivo ".obj". Veja https://en.wikipedia.org/wiki/Wavefront_.obj_file .
struct ObjModel
{
    tinyobj::attrib_t                 attrib;
    std::vector<tinyobj::shape_t>     shapes;
    std::vector<tinyobj::material_t>  materials;

    // Este construtor lê o modelo de um arquivo utilizando a biblioteca tinyobjloader.
    // Veja: https://github.com/syoyo/tinyobjloader
    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true)
    {
        printf("Carregando modelo \"%s\"... ", filename);

        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename, basepath, triangulate);

        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());

        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");

        printf("OK.\n");
    }
};


// Declaração de funções utilizadas para pilha de matrizes de modelagem.
void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4& M);

// Declaração de várias funções utilizadas em main().  Essas estão definidas
// logo após a definição de main() neste arquivo.
void BuildTrianglesAndAddToVirtualScene(ObjModel*); // Constrói representação de um ObjModel como malha de triângulos para renderização
void ComputeNormals(ObjModel* model); // Computa normais de um ObjModel, caso não existam.
void LoadShadersFromFiles(); // Carrega os shaders de vértice e fragmento, criando um programa de GPU
void LoadTextureImage(const char* filename); // Função que carrega imagens de textura
void DrawVirtualObject(const char* object_name); // Desenha um objeto armazenado em g_VirtualScene
GLuint LoadShader_Vertex(const char* filename);   // Carrega um vertex shader
GLuint LoadShader_Fragment(const char* filename); // Carrega um fragment shader
void LoadShader(const char* filename, GLuint shader_id); // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); // Cria um programa de GPU
void PrintObjModelInfo(ObjModel*); // Função para debugging

// Funções abaixo renderizam como texto na janela OpenGL algumas matrizes e
// outras informações do programa. Definidas após main().
void TextRendering_ShowModelViewProjection(GLFWwindow* window, glm::mat4 projection, glm::mat4 view, glm::mat4 model, glm::vec4 p_model);
void TextRendering_ShowEulerAngles(GLFWwindow* window);
void TextRendering_ShowProjection(GLFWwindow* window);
void TextRendering_ShowFramesPerSecond(GLFWwindow* window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ErrorCallback(int error, const char* description);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

glm::vec4 checkIntersection(glm::vec4 cameraPosition);
void bulletCollision();

// Definimos uma estrutura que armazenará dados necessários para renderizar
// cada objeto da cena virtual.
struct SceneObject
{
    std::string  name;        // Nome do objeto
    void*        first_index; // Índice do primeiro vértice dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    int          num_indices; // Número de índices do objeto dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    GLenum       rendering_mode; // Modo de rasterização (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
    GLuint       vertex_array_object_id; // ID do VAO onde estão armazenados os atributos do modelo
    glm::vec3    bbox_min; // Axis-Aligned Bounding Box do objeto
    glm::vec3    bbox_max;
};

// Abaixo definimos variáveis globais utilizadas em várias funções do código.

// A cena virtual é uma lista de objetos nomeados, guardados em um dicionário
// (map).  Veja dentro da função BuildTrianglesAndAddToVirtualScene() como que são incluídos
// objetos dentro da variável g_VirtualScene, e veja na função main() como
// estes são acessados.
std::map<std::string, SceneObject> g_VirtualScene;

// Pilha que guardará as matrizes de modelagem.
std::stack<glm::mat4>  g_MatrixStack;

// Razão de proporção da janela (largura/altura). Veja função FramebufferSizeCallback().
float g_ScreenRatio = 16.0f/9.0f;

// Ângulos de Euler que controlam a rotação de um dos cubos da cena virtual
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;

float PI = 3.141592f;
float EPSILON = 0.000000000001f;

// Variáveis que definem se certas teclas/botões estão sendo pressionados no momento atual
// Veja função MouseButtonCallback().
bool g_LeftMouseButtonPressed = false;
bool g_MiddleMouseButtonToggled = false;
bool g_RightMouseButtonPressed = false;
bool g_WKeyPressed = false;
bool g_AKeyPressed = false;
bool g_SKeyPressed = false;
bool g_DKeyPressed = false;
bool g_QKeyPressed = false;
bool g_EKeyPressed = false;
bool g_SpaceKeyPressed = false;
bool g_LeftShiftKeyPressed = false;

bool g_TargetLocked = false;

// Posições dos asteroides na cena
glm::vec4 g_AsteroidPos[5] = {
    glm::vec4(-100.0f,100.0f,0.0f,1.0f),
    glm::vec4(-35.0f,-50.0f,-240.0f,1.0f),
    glm::vec4(225.0f,0.0f,180.0f,1.0f),
    glm::vec4(43.0f,89.0f,-25.0f,1.0f),
    glm::vec4(-130.0f,-150.0f,230.0f,1.0f)};
int g_ClosestAsteroid = 0;
bool g_AsteroidVisible[5] = {true, true, true, true, true};

glm::dvec3 g_ControlPoints[4] = {
    //glm::dvec3(0.1,1.0,0.5),glm::dvec3(0.88,1.0,0.13),glm::dvec3(0.94,0.8,0.26),glm::dvec3(0.72,1.0,0.7)
    glm::dvec3(111.1,210.4,111.0),glm::dvec3(211.56,112.25,111.0),glm::dvec3(-50.33,50.99,51.0),glm::dvec3(40.78,92.85,31.0)
    //glm::dvec3(0.454,0.065,0.0),glm::dvec3(0.76,0.635,0.0),glm::dvec3(0.06,0.314,0.0),glm::dvec3(0.33,0.89,0.0)
};
int g_BezierOrientation = 1;
double g_TimePassed = 0.0;
double g_BezierDisplacementX = 0.0;
double g_BezierDisplacementY = 0.0;
double g_BezierDisplacementZ = 0.0;

bool g_BulletVisible = false;
glm::vec4 g_BulletOrigin;
glm::vec4 g_BulletPosition;
glm::vec4 g_BulletDirection;
float g_BulletDistance = 0.0f;

// Variáveis que definem onde a câmera está olhando em coordenadas esféricas, controladas pelo usuário através do mouse
// (veja função CursorPosCallback()). A posição efetiva é calculada dentro da função main(), dentro do loop de renderização.
float g_CameraTheta = 0.0f; // Ângulo no plano ZX em relação ao eixo Z
float g_CameraPhi = PI/2;   // Ângulo em relação ao eixo Y
float g_CameraDistance = 3.5f; // Distância da câmera para a origem

// Abaixo definimos as varáveis que efetivamente definem a câmera virtual.
// Veja slides 172-182 do documento "Aula_08_Sistemas_de_Coordenadas.pdf".
glm::vec4 g_CameraPosition  = glm::vec4(0.0f,0.0f,0.0f,1.0f); // Ponto "c", centro da câmera
glm::vec4 g_CameraLookAt    = glm::vec4(g_CameraPosition.x + g_CameraDistance*sin(g_CameraPhi)*sin(g_CameraTheta), // Ponto "l", para onde a câmera está olhando
                                        g_CameraPosition.y + g_CameraDistance*cos(g_CameraPhi),
                                        g_CameraPosition.z + g_CameraDistance*sin(g_CameraPhi)*cos(g_CameraTheta),1.0f);
glm::vec4 g_CameraViewVector = g_CameraLookAt - g_CameraPosition; // Vetor "view", sentido para onde a câmera está virada
glm::vec4 g_CameraUpVector   = glm::vec4(0.0f,1.0f,0.0f,0.0f); // Vetor "up" fixado para apontar para o "céu" (eixo Y global)
glm::vec4 g_CameraRightVector = normalize(crossproduct(g_CameraViewVector, g_CameraUpVector));

// Variável que controla o tipo de projeção utilizada: perspectiva ou ortográfica.
bool g_UsePerspectiveProjection = true;

// Variável que controla se o texto informativo será mostrado na tela.
bool g_ShowInfoText = false;

// Variáveis que definem um programa de GPU (shaders). Veja função LoadShadersFromFiles().
GLuint vertex_shader_id;
GLuint fragment_shader_id;
GLuint program_id = 0;
GLint model_uniform;
GLint view_uniform;
GLint projection_uniform;
GLint object_id_uniform;
GLint bbox_min_uniform;
GLint bbox_max_uniform;

// Número de texturas carregadas pela função LoadTextureImage()
GLuint g_NumLoadedTextures = 0;

int main(int argc, char* argv[])
{
    // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do sistema operacional, onde poderemos renderizar com OpenGL.
    int success = glfwInit();
    if (!success)
    {
        fprintf(stderr, "ERROR: glfwInit() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos o callback para impressão de erros da GLFW no terminal
    glfwSetErrorCallback(ErrorCallback);

    // Pedimos para utilizar OpenGL versão 3.3 (ou superior)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // Pedimos para utilizar o perfil "core", isto é, utilizaremos somente as funções modernas de OpenGL.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Criamos uma janela do sistema operacional, com 1280 colunas e 720 linhas de pixels
    GLFWwindow* window;
    window = glfwCreateWindow(1280, 720, "INF01047 - 228509 - Pedro Caetano de Abreu Teixeira", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos a função de callback que será chamada sempre que o usuário
    // pressionar alguma tecla do teclado ...
    glfwSetKeyCallback(window, KeyCallback);
    // ... ou clicar os botões do mouse ...
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    // ... ou movimentar o cursor do mouse em cima da janela ...
    glfwSetCursorPosCallback(window, CursorPosCallback);
    // ... ou rolar a "rodinha" do mouse.
    glfwSetScrollCallback(window, ScrollCallback);

    // Indicamos que as chamadas OpenGL deverão renderizar nesta janela
    glfwMakeContextCurrent(window);

    // Carregamento de todas funções definidas por OpenGL 3.3, utilizando a biblioteca GLAD.
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    // Definimos a função de callback que será chamada sempre que a janela for redimensionada, por consequência
    // alterando o tamanho do "framebuffer" (região de memória onde são armazenados os pixels da imagem).
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 1280, 720); // Forçamos a chamada do callback acima, para definir g_ScreenRatio.

    // Imprimimos no terminal informações sobre a GPU do sistema
    const GLubyte *vendor      = glGetString(GL_VENDOR);
    const GLubyte *renderer    = glGetString(GL_RENDERER);
    const GLubyte *glversion   = glGetString(GL_VERSION);
    const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion, glslversion);

    // Carregamos os shaders de vértices e de fragmentos que serão utilizados para renderização.
    // Veja slides 217-219 do documento "Aula_03_Rendering_Pipeline_Grafico.pdf".
    LoadShadersFromFiles();

    // Carregamos imagens para serem utilizadas como textura
    LoadTextureImage("../../data/ship.jpg"); // TextureImage0
    LoadTextureImage("../../data/ship.jpg"); // TextureImage1
    LoadTextureImage("../../data/Down_1K_TEX.png"); // TextureImage2
    LoadTextureImage("../../data/Up_1K_TEX.png"); // TextureImage3
    LoadTextureImage("../../data/Front_1K_TEX.png"); // TextureImage4
    LoadTextureImage("../../data/Back_1K_TEX.png"); // TextureImage5
    LoadTextureImage("../../data/Left_1K_TEX.png"); // TextureImage6
    LoadTextureImage("../../data/Right_1K_TEX.png"); // TextureImage7
    LoadTextureImage("../../data/2k_ceres_fictional.jpg"); // TextureImage8
    LoadTextureImage("../../data/Tropical.png"); // TextureImage8

    // Construímos a representação de objetos geométricos através de malhas de triângulos
    ObjModel spheremodel("../../data/sphere.obj");
    ComputeNormals(&spheremodel);
    BuildTrianglesAndAddToVirtualScene(&spheremodel);

    ObjModel shipmodel("../../data/ship.obj");
    ComputeNormals(&shipmodel);
    BuildTrianglesAndAddToVirtualScene(&shipmodel);

    ObjModel planemodel("../../data/plane.obj");
    ComputeNormals(&planemodel);
    BuildTrianglesAndAddToVirtualScene(&planemodel);

    ObjModel asteroidmodel("../../data/asteroid.obj");
    ComputeNormals(&asteroidmodel);
    BuildTrianglesAndAddToVirtualScene(&asteroidmodel);

    if ( argc > 1 )
    {
        ObjModel model(argv[1]);
        BuildTrianglesAndAddToVirtualScene(&model);
    }

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();

    // Habilitamos o Z-buffer. Veja slide 108 do documento "Aula_09_Projecoes.pdf".
    glEnable(GL_DEPTH_TEST);

    // Habilitamos o Backface Culling. Veja slides 22-34 do documento "Aula_13_Clipping_and_Culling.pdf".
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Variáveis auxiliares utilizadas para chamada à função TextRendering_ShowModelViewProjection(), armazenando matrizes 4x4.
    glm::mat4 the_projection;
    glm::mat4 the_model;
    glm::mat4 the_view;

    double t_prev = glfwGetTime();
    double t_now;
    double t_diff;
    // Ficamos em loop, renderizando, até que o usuário feche a janela
    while (!glfwWindowShouldClose(window))
    {
        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como branco.  Tal cor é definida como coeficientes RGBA: Red, Green, Blue, Alpha
        //           R     G     B     A
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima, e também resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo os shaders de vértice e fragmentos).
        glUseProgram(program_id);

        t_now = glfwGetTime();
        t_diff = t_now - t_prev;
        t_prev = t_now;

        // Movimentação da câmera de acordo com as teclas sendo pressionadas no momento.
        // Veja a função KeyCallback().
        float cameraSpeed = 0.05f;
        float rotationSpeed = 0.001f;
        g_CameraRightVector = normalize(crossproduct(g_CameraViewVector, g_CameraUpVector));

        if (g_WKeyPressed)
            g_CameraPosition += (cameraSpeed + (float) t_diff) * g_CameraViewVector;
        if (g_AKeyPressed)
            g_CameraPosition -= (cameraSpeed + (float) t_diff) * g_CameraRightVector;
        if (g_SKeyPressed)
            g_CameraPosition -= (cameraSpeed + (float) t_diff) * g_CameraViewVector;
        if (g_DKeyPressed)
            g_CameraPosition += (cameraSpeed + (float) t_diff) * g_CameraRightVector;
        if (g_QKeyPressed)
            g_CameraUpVector = Matrix_Rotate((rotationSpeed + (float) t_diff), g_CameraViewVector) * g_CameraUpVector;
        if (g_EKeyPressed)
            g_CameraUpVector = Matrix_Rotate((-rotationSpeed - (float) t_diff), g_CameraViewVector) * g_CameraUpVector;
        if (g_SpaceKeyPressed && !g_LeftShiftKeyPressed)
            g_CameraPosition += (cameraSpeed + (float) t_diff) * g_CameraUpVector;
        if (g_SpaceKeyPressed && g_LeftShiftKeyPressed)
            g_CameraPosition -= (cameraSpeed + (float) t_diff) * g_CameraUpVector;

        g_CameraPosition = checkIntersection(g_CameraPosition); //check for intersection here

        glm::vec4 oldViewVector = g_CameraViewVector;

        if(!g_MiddleMouseButtonToggled){
            // Computamos a direção da câmera utilizando coordenadas esféricas. As variáveis g_CameraDistance, g_CameraPhi, e g_CameraTheta são
            // controladas pelo mouse do usuário. Veja as funções CursorPosCallback() e ScrollCallback().
            float r = g_CameraDistance;
            float x = g_CameraPosition.x + r*sin(g_CameraPhi)*sin(g_CameraTheta);
            float z = g_CameraPosition.z + r*sin(g_CameraPhi)*cos(g_CameraTheta);
            float y = g_CameraPosition.y + r*cos(g_CameraPhi);

            g_CameraLookAt = glm::vec4(x,y,z,1.0f);

            g_CameraViewVector = normalize(g_CameraLookAt - g_CameraPosition);
        }
        else{
            if(!g_TargetLocked){
                g_ClosestAsteroid = -1;
                float distanceToAsteroid = distanceBetweenPoints(g_CameraPosition, g_AsteroidPos[0]);
                float distanceClosest = distanceToAsteroid;
                if(g_AsteroidVisible[0] == true)
                    g_ClosestAsteroid = 0;

                for(int i = 1; i <= 4; i++){
                    distanceToAsteroid = distanceBetweenPoints(g_CameraPosition, g_AsteroidPos[i]);
                    if(distanceToAsteroid <= distanceClosest && g_AsteroidVisible[i] == true){
                        distanceClosest = distanceToAsteroid;
                        g_ClosestAsteroid = i;
                    }
                }
                if(g_ClosestAsteroid > -1) {// If at least one asteroid is visible
                    g_CameraLookAt = g_AsteroidPos[g_ClosestAsteroid];
                    g_TargetLocked = true;
                }
                else{
                    g_MiddleMouseButtonToggled = false;
                }
            }
            else if(g_TargetLocked && (g_ClosestAsteroid == 0) && (g_AsteroidVisible[0] == true)) {
                g_CameraLookAt = glm::vec4(g_AsteroidPos[0].x + g_BezierDisplacementX,
                                           g_AsteroidPos[0].y + g_BezierDisplacementY,
                                           g_AsteroidPos[0].z + g_BezierDisplacementZ,
                                           1.0);
            }
            else if(g_TargetLocked && g_AsteroidVisible[g_ClosestAsteroid] == false){
                g_TargetLocked = false;
                g_MiddleMouseButtonToggled = false;
            }

            g_CameraViewVector = normalize(g_CameraLookAt - g_CameraPosition);
        }

        if(notEqual(oldViewVector, g_CameraViewVector, EPSILON)) // Se houve uma mudança no vetor view
        {
            // Pega o ângulo entre o vetor view novo e antigo
            float rotationAngle = acos(dotproduct(g_CameraViewVector, oldViewVector)/(norm(g_CameraViewVector)*norm(oldViewVector)));

            //PrintVector(oldViewVector);
            //PrintVector(g_CameraViewVector);
            //printf("u * v = %f, |u|*|v| = %f, theta = %f\n", dotproduct(g_CameraViewVector, oldViewVector), norm(g_CameraViewVector)*norm(oldViewVector), rotationAngle);
            //PrintVector(g_CameraUpVector);
            // Pega o eixo de rotação usado para mover o vetor view
            glm::vec4 rotationAxis = normalize(crossproduct(g_CameraViewVector, oldViewVector));

            // E rotaciona o vetor up em relação ao mesmo eixo e no mesmo ângulo
            g_CameraUpVector = g_CameraUpVector * Matrix_Rotate(rotationAngle, rotationAxis);
        }

        // Computamos a matriz "View" utilizando os parâmetros da câmera para definir o sistema de coordenadas da câmera.
        // Veja slide 186 do documento "Aula_08_Sistemas_de_Coordenadas.pdf".
        glm::mat4 view = Matrix_Camera_View(g_CameraPosition, g_CameraViewVector, g_CameraUpVector);

        // Agora computamos a matriz de projeção.
        glm::mat4 projection;

        // Note que, no sistema de coordenadas da câmera, os planos near e far estão no sentido negativo!
        // Veja slides 190-193 do documento "Aula_09_Projecoes.pdf".
        float nearplane = -0.1f;  // Posição do "near plane"
        float farplane  = -2400.0f; // Posição do "far plane"

        if (g_UsePerspectiveProjection)
        {
            // Projeção Perspectiva.
            // Para definição do field of view (FOV), veja slide 227 do documento "Aula_09_Projecoes.pdf".
            float field_of_view = 3.141592 / 3.0f;
            projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);
        }
        else
        {
            // Projeção Ortográfica.
            // Para definição dos valores l, r, b, t ("left", "right", "bottom", "top") PARA PROJEÇÃO ORTOGRÁFICA,
            //veja slide 236 do documento "Aula_09_Projecoes.pdf".
            // Para simular um "zoom" ortográfico, computamos o valor de "t" utilizando a variável g_CameraDistance.
            float t = 1.5f*g_CameraDistance/2.5f;
            float b = -t;
            float r = t*g_ScreenRatio;
            float l = -r;
            projection = Matrix_Orthographic(l, r, b, t, nearplane, farplane);
        }

        glm::mat4 model = Matrix_Identity(); // Transformação identidade de modelagem
        PushMatrix(model); // Guardamos matriz model atual na pilha

        // Enviamos as matrizes "view" e "projection" para a placa de vídeo (GPU).
        // Veja o arquivo "shader_vertex.glsl", onde estas são efetivamente aplicadas em todos os pontos.
        glUniformMatrix4fv(view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));

        #define SPHERE  0
        #define SHIP   1
        #define SKYBOX_BOTTOM 2
        #define SKYBOX_TOP 3
        #define SKYBOX_FRONT 4
        #define SKYBOX_BACK 5
        #define SKYBOX_LEFT 6
        #define SKYBOX_RIGHT 7
        #define TROPICAL 8
        #define ASTEROID 9
        #define BULLET 10

        // Desenhamos alguns asteroides

        // Esse asteroide se movimenta ao longo de uma curva de Bézier
        if(g_AsteroidVisible[0] == true){
            double tmp = glfwGetTime()/25 - g_TimePassed;
            double time;
            if(g_BezierOrientation == 1){
                time = tmp;
            }
            if(time >= 1 && g_BezierOrientation == 1){
                g_BezierOrientation = 0;
                g_TimePassed = glfwGetTime()/25;
                tmp = glfwGetTime()/25 - g_TimePassed;
            }
            if(g_BezierOrientation == 0){
                time = 1 - tmp;
            }
            if(time <= 0 && g_BezierOrientation == 0){
                g_BezierOrientation = 1;
                g_TimePassed = glfwGetTime()/25;
            }

            double mt = 1 - time;

            g_BezierDisplacementX = mt*mt*mt*g_ControlPoints[0].x +
                3*time*mt*mt*g_ControlPoints[1].x +
                3*time*time*mt*g_ControlPoints[2].x +
                time*time*time*g_ControlPoints[3].x;
            g_BezierDisplacementY = mt*mt*mt*g_ControlPoints[0].y +
                3*time*mt*mt*g_ControlPoints[1].y +
                3*time*time*mt*g_ControlPoints[2].y +
                time*time*time*g_ControlPoints[3].y;
            g_BezierDisplacementZ = mt*mt*mt*g_ControlPoints[0].z +
                3*time*mt*mt*g_ControlPoints[1].z +
                3*time*time*mt*g_ControlPoints[2].z +
                time*time*time*g_ControlPoints[3].z;

            model = Matrix_Translate(g_AsteroidPos[0].x + g_BezierDisplacementX,g_AsteroidPos[0].y + g_BezierDisplacementY,g_AsteroidPos[0].z + g_BezierDisplacementZ)
                  * Matrix_Scale(30.00f, 30.00f, 20.00f);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ASTEROID);
            DrawVirtualObject("asteroid");
        }

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        if(g_AsteroidVisible[1] == true){
            model = Matrix_Translate(g_AsteroidPos[1].x,g_AsteroidPos[1].y,g_AsteroidPos[1].z)
                  * Matrix_Rotate_Y(2.3f)
                  * Matrix_Scale(30.00f, 30.00f, 30.00f);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ASTEROID);
            DrawVirtualObject("asteroid");
        }

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        if(g_AsteroidVisible[2] == true){
            model = Matrix_Translate(g_AsteroidPos[2].x,g_AsteroidPos[2].y,g_AsteroidPos[2].z)
                  * Matrix_Rotate_Z(-0.4f)
                  * Matrix_Rotate_X(1.5f)
                  * Matrix_Scale(35.00f, 20.00f, 35.00f);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ASTEROID);
            DrawVirtualObject("asteroid");
        }

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        if(g_AsteroidVisible[3] == true){
            model = Matrix_Translate(g_AsteroidPos[3].x,g_AsteroidPos[3].y,g_AsteroidPos[3].z)
                  * Matrix_Rotate_X(1.2f)
                  * Matrix_Scale(20.00f, 20.00f, 20.00f);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ASTEROID);
            DrawVirtualObject("asteroid");
        }

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        if(g_AsteroidVisible[4] == true){
            model = Matrix_Translate(g_AsteroidPos[4].x,g_AsteroidPos[4].y,g_AsteroidPos[4].z)
                  * Matrix_Rotate_Z(0.6f)
                  * Matrix_Scale(25.00f, 25.00f, 25.00f);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ASTEROID);
            DrawVirtualObject("asteroid");
        }

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        // Desenhamos o modelo do planeta pequeno
        model = Matrix_Translate(-245.0f,170.0f,0.0f)
              * Matrix_Rotate_Z(0.6f)
              * Matrix_Rotate_X(0.2f)
              * Matrix_Rotate_Y((float)glfwGetTime() * 0.1f)
              * Matrix_Scale(10.00f, 10.00f, 10.00f);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, SPHERE);
        DrawVirtualObject("sphere");

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        // Desenhamos o modelo do planeta grande
        model = Matrix_Translate(0.0f,-250.0f,0.0f)
              * Matrix_Scale(200.00f, 200.00f, 200.00f);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, TROPICAL);
        DrawVirtualObject("sphere");

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        // Desenhamos o modelo do chão da skybox
        model = Matrix_Translate(0.0f, -1000.0f, 0.0f)
              * Matrix_Rotate_Y(PI/2);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, SKYBOX_BOTTOM);
        DrawVirtualObject("plane");

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        // Desenhamos o modelo do teto da skybox
        model = Matrix_Translate(0.0f, 1000.0f, 0.0f)
              * Matrix_Rotate_Z(PI)
              * Matrix_Rotate_Y(PI/2);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, SKYBOX_TOP);
        DrawVirtualObject("plane");

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        // Desenhamos o modelo da parede da frente da skybox
        model = Matrix_Translate(1000.0f, 0.0f, 0.0f)
              * Matrix_Rotate_Z(PI/2)
              * Matrix_Rotate_Y(PI/2);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, SKYBOX_FRONT);
        DrawVirtualObject("plane");

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        // Desenhamos o modelo da parede de trás da skybox
        model = Matrix_Translate(-1000.0f, 0.0f, 0.0f)
              * Matrix_Rotate_Z(3*PI/2)
              * Matrix_Rotate_Y(3*PI/2);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, SKYBOX_BACK);
        DrawVirtualObject("plane");

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        // Desenhamos o modelo da parede da esquerda da skybox
        model = Matrix_Translate(0.0f, 0.0f, -1000.0f)
              * Matrix_Rotate_X(PI/2)
              * Matrix_Rotate_Y(PI);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, SKYBOX_LEFT);
        DrawVirtualObject("plane");

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        // Desenhamos o modelo da parede da direita da skybox
        model = Matrix_Translate(0.0f, 0.0f, 1000.0f)
              * Matrix_Rotate_X(3*PI/2);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, SKYBOX_RIGHT);
        DrawVirtualObject("plane");

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha


        // Desenhamos o modelo do personagem
        glm::vec4 direction = normalize(g_CameraViewVector);
        glm::vec4 downVector = normalize(-g_CameraUpVector);

        model = model
              * Matrix_Translate(downVector.x/20, downVector.y/20, downVector.z/20) // Coloca a nave um pouco abaixo da câmera
              * Matrix_Translate(direction.x/5, direction.y/5, direction.z/5) // Coloca a nave um pouco à frente da câmera
              * Matrix_Translate(g_CameraPosition.x, g_CameraPosition.y, g_CameraPosition.z) // Traz a nave para a posição da câmera
              * Matrix_Rotate(PI/2 - 1.1*g_CameraPhi, g_CameraRightVector)
              * Matrix_Rotate(1.1*g_CameraTheta, g_CameraUpVector)
              * Matrix_Scale(0.01f, 0.01f, 0.01f); // Reduz o tamanho do modelo da nave

        //oldViewVector = glm::vec4(0.0f, 0.0f, 3.5f, 0.0f);
        // Pega o ângulo entre o vetor view novo e antigo
        //float rotationAngle = acos(dotproduct(g_CameraViewVector, oldViewVector)/(norm(g_CameraViewVector)*norm(oldViewVector)));
        //glm::vec4 rotationAxis = normalize(crossproduct(g_CameraViewVector, oldViewVector));
        // E rotaciona o vetor up em relação ao mesmo eixo e no mesmo ângulo
        //model = model * Matrix_Rotate(rotationAngle, rotationAxis);

        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, SHIP);
        DrawVirtualObject("ship");

        PopMatrix(model); // Tiramos da pilha a matriz identidade guardada anteriormente
        PushMatrix(model); // Guardamos matriz model atual na pilha

        // Desenhamos a bala atirada pela nave
        if(g_LeftMouseButtonPressed == true && g_BulletVisible == false){
            g_BulletVisible = true;
            g_BulletOrigin = g_CameraPosition + 3.0f*g_CameraViewVector + 0.7f*downVector;
            g_BulletPosition = g_BulletOrigin;
            g_BulletDirection = g_CameraViewVector;
            g_BulletDistance = 0.0f;
        }
        if(g_BulletVisible == true){
            g_BulletPosition += (cameraSpeed + (float) t_diff) * g_BulletDirection;
            g_BulletDistance = distanceBetweenPoints(g_BulletOrigin, g_BulletPosition);

            model = Matrix_Translate(g_BulletPosition.x,g_BulletPosition.y,g_BulletPosition.z)
                  * Matrix_Scale(0.1f, 0.1f, 0.1f);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, BULLET);
            DrawVirtualObject("sphere");

            if(g_BulletDistance >= 60.0f)
                g_BulletVisible = false;
        }

        bulletCollision(); // Checa se a bala atingiu algum dos asteroides em seu trajeto

        // Pegamos um vértice com coordenadas de modelo (0.5, 0.5, 0.5, 1) e o passamos por todos os sistemas de coordenadas armazenados nas
        // matrizes the_model, the_view, e the_projection; e escrevemos na tela as matrizes e pontos resultantes dessas transformações.
        glm::vec4 p_model(0.5f, 0.5f, 0.5f, 1.0f);
        TextRendering_ShowModelViewProjection(window, projection, view, model, p_model);

        // Imprimimos na tela os ângulos de Euler que controlam a rotação do terceiro cubo.
        TextRendering_ShowEulerAngles(window);

        // Imprimimos na informação sobre a matriz de projeção sendo utilizada.
        TextRendering_ShowProjection(window);

        // Imprimimos na tela informação sobre o número de quadros renderizados
        // por segundo (frames per second).
        TextRendering_ShowFramesPerSecond(window);

        // O framebuffer onde OpenGL executa as operações de renderização não é o mesmo que está sendo mostrado para o usuário, caso contrário
        // seria possível ver artefatos conhecidos como "screen tearing". A chamada abaixo faz a troca dos buffers, mostrando para o usuário
        // tudo que foi renderizado pelas funções acima.
        // Veja o link: https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics
        glfwSwapBuffers(window);

        // Verificamos com o sistema operacional se houve alguma interação do usuário (teclado, mouse, ...).
        // Caso positivo, as funções de callback definidas anteriormente usando glfwSet*Callback() serão chamadas pela biblioteca GLFW.
        glfwPollEvents();
    }

    // Finalizamos o uso dos recursos do sistema operacional.
    glfwTerminate();

    // Fim do programa
    return 0;
}

// Função que carrega uma imagem para ser utilizada como textura
void LoadTextureImage(const char* filename)
{
    printf("Carregando imagem \"%s\"... ", filename);

    // Primeiro fazemos a leitura da imagem do disco
    stbi_set_flip_vertically_on_load(true);
    int width;
    int height;
    int channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);

    if ( data == NULL )
    {
        fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }

    printf("OK (%dx%d).\n", width, height);

    // Agora criamos objetos na GPU com OpenGL para armazenar a textura
    GLuint texture_id;
    GLuint sampler_id;
    glGenTextures(1, &texture_id);
    glGenSamplers(1, &sampler_id);

    // Veja slide 100 do documento "Aula_20_e_21_Mapeamento_de_Texturas.pdf"
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Parâmetros de amostragem da textura. Falaremos sobre eles em uma próxima aula.
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Agora enviamos a imagem lida do disco para a GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    GLuint textureunit = g_NumLoadedTextures;
    glActiveTexture(GL_TEXTURE0 + textureunit);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindSampler(textureunit, sampler_id);

    stbi_image_free(data);

    g_NumLoadedTextures += 1;
}

// Função que desenha um objeto armazenado em g_VirtualScene. Veja definição dos objetos na função BuildTrianglesAndAddToVirtualScene().
void DrawVirtualObject(const char* object_name)
{
    // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
    // vértices apontados pelo VAO criado pela função BuildTrianglesAndAddToVirtualScene(). Veja
    // comentários detalhados dentro da definição de BuildTrianglesAndAddToVirtualScene().
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);

    // Setamos as variáveis "bbox_min" e "bbox_max" do fragment shader
    // com os parâmetros da axis-aligned bounding box (AABB) do modelo.
    glm::vec3 bbox_min = g_VirtualScene[object_name].bbox_min;
    glm::vec3 bbox_max = g_VirtualScene[object_name].bbox_max;
    glUniform4f(bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

    // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
    // apontados pelo VAO como linhas. Veja a definição de
    // g_VirtualScene[""] dentro da função BuildTrianglesAndAddToVirtualScene(), e veja
    // a documentação da função glDrawElements() em
    // http://docs.gl/gl3/glDrawElements.
    glDrawElements(
        g_VirtualScene[object_name].rendering_mode,
        g_VirtualScene[object_name].num_indices,
        GL_UNSIGNED_INT,
        (void*)g_VirtualScene[object_name].first_index
    );

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Função que carrega os shaders de vértices e de fragmentos que serão
// utilizados para renderização. Veja slides 217-219 do documento "Aula_03_Rendering_Pipeline_Grafico.pdf".
//
void LoadShadersFromFiles()
{
    // Note que o caminho para os arquivos "shader_vertex.glsl" e
    // "shader_fragment.glsl" estão fixados, sendo que assumimos a existência
    // da seguinte estrutura no sistema de arquivos:
    vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");

    // Deletamos o programa de GPU anterior, caso ele exista.
    if ( program_id != 0 )
        glDeleteProgram(program_id);

    // Criamos um programa de GPU utilizando os shaders carregados acima.
    program_id = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
    // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
    // (GPU)! Veja arquivo "shader_vertex.glsl" e "shader_fragment.glsl".
    model_uniform           = glGetUniformLocation(program_id, "model"); // Variável da matriz "model"
    view_uniform            = glGetUniformLocation(program_id, "view"); // Variável da matriz "view" em shader_vertex.glsl
    projection_uniform      = glGetUniformLocation(program_id, "projection"); // Variável da matriz "projection" em shader_vertex.glsl
    object_id_uniform       = glGetUniformLocation(program_id, "object_id"); // Variável "object_id" em shader_fragment.glsl
    bbox_min_uniform        = glGetUniformLocation(program_id, "bbox_min");
    bbox_max_uniform        = glGetUniformLocation(program_id, "bbox_max");

    // Variáveis em "shader_fragment.glsl" para acesso das imagens de textura
    glUseProgram(program_id);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage2"), 2);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage3"), 3);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage4"), 4);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage5"), 5);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage6"), 6);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage7"), 7);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage8"), 8);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage9"), 9);
    glUseProgram(0);
}

// Função que pega a matriz M e guarda a mesma no topo da pilha
void PushMatrix(glm::mat4 M)
{
    g_MatrixStack.push(M);
}

// Função que remove a matriz atualmente no topo da pilha e armazena a mesma na variável M
void PopMatrix(glm::mat4& M)
{
    if ( g_MatrixStack.empty() )
    {
        M = Matrix_Identity();
    }
    else
    {
        M = g_MatrixStack.top();
        g_MatrixStack.pop();
    }
}

// Função que computa as normais de um ObjModel, caso elas não tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel* model)
{
    if ( !model->attrib.normals.empty() )
        return;

    // Primeiro computamos as normais para todos os TRIÂNGULOS.
    // Segundo, computamos as normais dos VÉRTICES através do método proposto
    // por Gouraud, onde a normal de cada vértice vai ser a média das normais de
    // todas as faces que compartilham este vértice.

    size_t num_vertices = model->attrib.vertices.size() / 3;

    std::vector<int> num_triangles_per_vertex(num_vertices, 0);
    std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f,0.0f,0.0f,0.0f));

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            glm::vec4  vertices[3];
            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                vertices[vertex] = glm::vec4(vx,vy,vz,1.0);
            }

            const glm::vec4  a = vertices[0];
            const glm::vec4  b = vertices[1];
            const glm::vec4  c = vertices[2];

            // PREENCHA AQUI o cálculo da normal de um triângulo cujos vértices
            // estão nos pontos "a", "b", e "c", definidos no sentido anti-horário.
            const glm::vec4  n = crossproduct(b - a, c - a);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                num_triangles_per_vertex[idx.vertex_index] += 1;
                vertex_normals[idx.vertex_index] += n;
                model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index = idx.vertex_index;
            }
        }
    }

    model->attrib.normals.resize( 3*num_vertices );

    for (size_t i = 0; i < vertex_normals.size(); ++i)
    {
        glm::vec4 n = vertex_normals[i] / (float)num_triangles_per_vertex[i];
        n /= norm(n);
        model->attrib.normals[3*i + 0] = n.x;
        model->attrib.normals[3*i + 1] = n.y;
        model->attrib.normals[3*i + 2] = n.z;
    }
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel* model)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float>  model_coefficients;
    std::vector<float>  normal_coefficients;
    std::vector<float>  texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        const float minval = std::numeric_limits<float>::min();
        const float maxval = std::numeric_limits<float>::max();

        glm::vec3 bbox_min = glm::vec3(maxval,maxval,maxval);
        glm::vec3 bbox_max = glm::vec3(minval,minval,minval);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];

                indices.push_back(first_index + 3*triangle + vertex);

                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                //printf("tri %d vert %d = (%.2f, %.2f, %.2f)\n", (int)triangle, (int)vertex, vx, vy, vz);
                model_coefficients.push_back( vx ); // X
                model_coefficients.push_back( vy ); // Y
                model_coefficients.push_back( vz ); // Z
                model_coefficients.push_back( 1.0f ); // W

                bbox_min.x = std::min(bbox_min.x, vx);
                bbox_min.y = std::min(bbox_min.y, vy);
                bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx);
                bbox_max.y = std::max(bbox_max.y, vy);
                bbox_max.z = std::max(bbox_max.z, vz);

                // Inspecionando o código da tinyobjloader, o aluno Bernardo
                // Sulzbach (2017/1) apontou que a maneira correta de testar se
                // existem normais e coordenadas de textura no ObjModel é
                // comparando se o índice retornado é -1. Fazemos isso abaixo.

                if ( idx.normal_index != -1 )
                {
                    const float nx = model->attrib.normals[3*idx.normal_index + 0];
                    const float ny = model->attrib.normals[3*idx.normal_index + 1];
                    const float nz = model->attrib.normals[3*idx.normal_index + 2];
                    normal_coefficients.push_back( nx ); // X
                    normal_coefficients.push_back( ny ); // Y
                    normal_coefficients.push_back( nz ); // Z
                    normal_coefficients.push_back( 0.0f ); // W
                }

                if ( idx.texcoord_index != -1 )
                {
                    const float u = model->attrib.texcoords[2*idx.texcoord_index + 0];
                    const float v = model->attrib.texcoords[2*idx.texcoord_index + 1];
                    texture_coefficients.push_back( u );
                    texture_coefficients.push_back( v );
                }
            }
        }

        size_t last_index = indices.size() - 1;

        SceneObject theobject;
        theobject.name           = model->shapes[shape].name;
        theobject.first_index    = (void*)first_index; // Primeiro índice
        theobject.num_indices    = last_index - first_index + 1; // Número de indices
        theobject.rendering_mode = GL_TRIANGLES;       // Índices correspondem ao tipo de rasterização GL_TRIANGLES.
        theobject.vertex_array_object_id = vertex_array_object_id;

        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;

        g_VirtualScene[model->shapes[shape].name] = theobject;
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0; // "(location = 0)" em "shader_vertex.glsl"
    GLint  number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if ( !normal_coefficients.empty() )
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        location = 1; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if ( !texture_coefficients.empty() )
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        location = 2; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint indices_id;
    glGenBuffers(1, &indices_id);

    // "Ligamos" o buffer. Note que o tipo agora é GL_ELEMENT_ARRAY_BUFFER.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!
    //

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Carrega um Vertex Shader de um arquivo GLSL. Veja definição de LoadShader() abaixo.
GLuint LoadShader_Vertex(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos vértices.
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, vertex_shader_id);

    // Retorna o ID gerado acima
    return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo GLSL . Veja definição de LoadShader() abaixo.
GLuint LoadShader_Fragment(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos fragmentos.
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, fragment_shader_id);

    // Retorna o ID gerado acima
    return fragment_shader_id;
}

// Função auxilar, utilizada pelas duas funções acima. Carrega código de GPU de
// um arquivo GLSL e faz sua compilação.
void LoadShader(const char* filename, GLuint shader_id)
{
    // Lemos o arquivo de texto indicado pela variável "filename"
    // e colocamos seu conteúdo em memória, apontado pela variável
    // "shader_string".
    std::ifstream file;
    try {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    } catch ( std::exception& e ) {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar* shader_string = str.c_str();
    const GLint   shader_string_length = static_cast<GLint>( str.length() );

    // Define o código do shader GLSL, contido na string "shader_string"
    glShaderSource(shader_id, 1, &shader_string, &shader_string_length);

    // Compila o código do shader GLSL (em tempo de execução)
    glCompileShader(shader_id);

    // Verificamos se ocorreu algum erro ou "warning" durante a compilação
    GLint compiled_ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled_ok);

    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    // Alocamos memória para guardar o log de compilação.
    // A chamada "new" em C++ é equivalente ao "malloc()" do C.
    GLchar* log = new GLchar[log_length];
    glGetShaderInfoLog(shader_id, log_length, &log_length, log);

    // Imprime no terminal qualquer erro ou "warning" de compilação
    if ( log_length != 0 )
    {
        std::string  output;

        if ( !compiled_ok )
        {
            output += "ERROR: OpenGL compilation of \"";
            output += filename;
            output += "\" failed.\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }
        else
        {
            output += "WARNING: OpenGL compilation of \"";
            output += filename;
            output += "\".\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }

        fprintf(stderr, "%s", output.c_str());
    }

    // A chamada "delete" em C++ é equivalente ao "free()" do C
    delete [] log;
}

// Esta função cria um programa de GPU, o qual contém obrigatoriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
    // Criamos um identificador (ID) para este programa de GPU
    GLuint program_id = glCreateProgram();

    // Definição dos dois shaders GLSL que devem ser executados pelo programa
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    // Linkagem dos shaders acima ao programa
    glLinkProgram(program_id);

    // Verificamos se ocorreu algum erro durante a linkagem
    GLint linked_ok = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked_ok);

    // Imprime no terminal qualquer erro de linkagem
    if ( linked_ok == GL_FALSE )
    {
        GLint log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

        // Alocamos memória para guardar o log de compilação.
        // A chamada "new" em C++ é equivalente ao "malloc()" do C.
        GLchar* log = new GLchar[log_length];

        glGetProgramInfoLog(program_id, log_length, &log_length, log);

        std::string output;

        output += "ERROR: OpenGL linking of program failed.\n";
        output += "== Start of link log\n";
        output += log;
        output += "\n== End of link log\n";

        // A chamada "delete" em C++ é equivalente ao "free()" do C
        delete [] log;

        fprintf(stderr, "%s", output.c_str());
    }

    // Os "Shader Objects" podem ser marcados para deleção após serem linkados
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    // Retornamos o ID gerado acima
    return program_id;
}

// Definição da função que será chamada sempre que a janela do sistema
// operacional for redimensionada, por consequência alterando o tamanho do
// "framebuffer" (região de memória onde são armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    // Indicamos que queremos renderizar em toda região do framebuffer. A
    // função "glViewport" define o mapeamento das "normalized device
    // coordinates" (NDC) para "pixel coordinates".  Essa é a operação de
    // "Screen Mapping" ou "Viewport Mapping" vista em aula (slides 33-44 do documento "Aula_07_Transformacoes_Geometricas_3D.pdf").
    glViewport(0, 0, width, height);

    // Atualizamos também a razão que define a proporção da janela (largura /
    // altura), a qual será utilizada na definição das matrizes de projeção,
    // tal que não ocorra distorções durante o processo de "Screen Mapping"
    // acima, quando NDC é mapeado para coordenadas de pixels. Veja slide 227 do documento "Aula_09_Projecoes.pdf".
    //
    // O cast para float é necessário pois números inteiros são arredondados ao
    // serem divididos!
    g_ScreenRatio = (float)width / height;
}

// Variáveis globais que armazenam a última posição do cursor do mouse, para
// que possamos calcular quanto que o mouse se movimentou entre dois instantes
// de tempo. Utilizadas no callback CursorPosCallback() abaixo.
double g_LastCursorPosX, g_LastCursorPosY;

// Função callback chamada sempre que o usuário aperta algum dos botões do mouse
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão direito do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_LeftMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_RightMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_RightMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        if(g_MiddleMouseButtonToggled == true){
            g_MiddleMouseButtonToggled = false;
            g_TargetLocked = false;
        }
        else{
            g_MiddleMouseButtonToggled = true;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        g_LeftMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_LeftMouseButtonPressed = false;
    }
}

// Função callback chamada sempre que o usuário movimentar o cursor do mouse em
// cima da janela OpenGL.
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    // Abaixo executamos o seguinte: caso o botão direito do mouse esteja
    // pressionado, computamos quanto que o mouse se movimento desde o último
    // instante de tempo, e usamos esta movimentação para atualizar os
    // parâmetros que definem a posição da câmera dentro da cena virtual.
    // Assim, temos que o usuário consegue controlar a câmera.

    if (g_RightMouseButtonPressed && !g_MiddleMouseButtonToggled)
    {
        // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;

        // Atualizamos parâmetros da câmera com os deslocamentos
        g_CameraTheta -= 0.01f*dx;
        g_CameraPhi   += 0.01f*dy;

        // Em coordenadas esféricas, o ângulo phi deve ficar entre 0 e +pi.
        float phimax = 29*PI/32;
        float phimin = 3*PI/32;

        if (g_CameraPhi >= phimax)
            g_CameraPhi = phimax;
        if (g_CameraPhi < phimin)
            g_CameraPhi = phimin;

        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
    }

}

// Função callback chamada sempre que o usuário movimenta a "rodinha" do mouse.
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Atualizamos a distância da câmera para a origem utilizando a
    // movimentação da "rodinha", simulando um ZOOM.
    g_CameraDistance -= 0.1f*yoffset;

    // Uma câmera look-at nunca pode estar exatamente "em cima" do ponto para
    // onde ela está olhando, pois isto gera problemas de divisão por zero na
    // definição do sistema de coordenadas da câmera. Isto é, a variável abaixo
    // nunca pode ser zero. Versões anteriores deste código possuíam este bug,
    // o qual foi detectado pelo aluno Vinicius Fraga (2017/2).
    const float verysmallnumber = std::numeric_limits<float>::epsilon();
    if (g_CameraDistance < verysmallnumber)
        g_CameraDistance = verysmallnumber;
}

// Definição da função que será chamada sempre que o usuário pressionar alguma
// tecla do teclado. Veja http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mod)
{
    // ==============
    // Não modifique este loop! Ele é utilizando para correção automatizada dos
    // laboratórios. Deve ser sempre o primeiro comando desta função KeyCallback().
    for (int i = 0; i < 10; ++i)
        if (key == GLFW_KEY_0 + i && action == GLFW_PRESS && mod == GLFW_MOD_SHIFT)
            std::exit(100 + i);
    // ==============

    // Se o usuário pressionar a tecla ESC, fechamos a janela.
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // O código abaixo implementa a seguinte lógica:
    //   Se apertar tecla X       então g_AngleX += delta;
    //   Se apertar tecla shift+X então g_AngleX -= delta;
    //   Se apertar tecla Y       então g_AngleY += delta;
    //   Se apertar tecla shift+Y então g_AngleY -= delta;
    //   Se apertar tecla Z       então g_AngleZ += delta;
    //   Se apertar tecla shift+Z então g_AngleZ -= delta;

    float delta = 3.141592 / 16; // 22.5 graus, em radianos.

    if (key == GLFW_KEY_X && action == GLFW_PRESS)
    {
        g_AngleX += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }

    if (key == GLFW_KEY_Y && action == GLFW_PRESS)
    {
        g_AngleY += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }
    if (key == GLFW_KEY_Z && action == GLFW_PRESS)
    {
        g_AngleZ += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }

    // Se o usuário apertar a tecla espaço, resetamos os ângulos de Euler para zero.
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        g_AngleX = 0.0f;
        g_AngleY = 0.0f;
        g_AngleZ = 0.0f;
    }

    // Se o usuário apertar a tecla P, utilizamos projeção perspectiva.
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = true;
    }

    // Se o usuário apertar a tecla O, utilizamos projeção ortográfica.
    if (key == GLFW_KEY_O && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = false;
    }

    // Se o usuário apertar a tecla H, fazemos um "toggle" do texto informativo mostrado na tela.
    if (key == GLFW_KEY_H && action == GLFW_PRESS)
    {
        g_ShowInfoText = !g_ShowInfoText;
    }

    // Se o usuário apertar a tecla R, recarregamos os shaders dos arquivos "shader_fragment.glsl" e "shader_vertex.glsl".
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        LoadShadersFromFiles();
        fprintf(stdout,"Shaders recarregados!\n");
        fflush(stdout);
    }

    // As linhas de código abaixam detectam quando o usuário pressiona qualquer uma das teclas WASD do teclado, que então movimentam a câmera.
    if (key == GLFW_KEY_W && action == GLFW_PRESS)
        g_WKeyPressed = true;
    if (key == GLFW_KEY_W && action == GLFW_RELEASE)
        g_WKeyPressed = false;

    if (key == GLFW_KEY_A && action == GLFW_PRESS)
        g_AKeyPressed = true;
    if (key == GLFW_KEY_A && action == GLFW_RELEASE)
        g_AKeyPressed = false;

    if (key == GLFW_KEY_S && action == GLFW_PRESS)
        g_SKeyPressed = true;
    if (key == GLFW_KEY_S && action == GLFW_RELEASE)
        g_SKeyPressed = false;

    if (key == GLFW_KEY_D && action == GLFW_PRESS)
        g_DKeyPressed = true;
    if (key == GLFW_KEY_D && action == GLFW_RELEASE)
        g_DKeyPressed = false;

    if (key == GLFW_KEY_Q && action == GLFW_PRESS)
        g_QKeyPressed = true;
    if (key == GLFW_KEY_Q && action == GLFW_RELEASE)
        g_QKeyPressed = false;

    if (key == GLFW_KEY_E && action == GLFW_PRESS)
        g_EKeyPressed = true;
    if (key == GLFW_KEY_E && action == GLFW_RELEASE)
        g_EKeyPressed = false;

    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        g_SpaceKeyPressed = true;
    if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE)
        g_SpaceKeyPressed = false;

    if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_PRESS)
        g_LeftShiftKeyPressed = true;
    if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_RELEASE)
        g_LeftShiftKeyPressed = false;
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char* description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

glm::vec4 checkIntersection(glm::vec4 cameraPosition) {
    // Impede que o usuário chegue próximo dos 6 planos que compõem a skybox
    if( cameraPosition.x > 250.0f )
        cameraPosition.x = 240.0f;
    if( cameraPosition.x < -250.0f )
        cameraPosition.x = -240.0f;
    if( cameraPosition.y > 250.0f )
        cameraPosition.y = 240.0f;
    if( cameraPosition.y < -250.0f )
        cameraPosition.y = -240.0f;
    if( cameraPosition.z > 250.0f )
        cameraPosition.z = 240.0f;
    if( cameraPosition.z < -250.0f )
        cameraPosition.z = -240.0f;

    // Impede que o usuário entre em qualquer um dos dois planetas na cena
    glm::vec3 bbox_minSphere = g_VirtualScene["sphere"].bbox_min;
    glm::vec3 bbox_maxSphere = g_VirtualScene["sphere"].bbox_max;

    // ------- Planeta 1
    float radiusBigPlanet = 200.0f; // Porque o planeta grande foi escalado 200 vezes
    glm::vec4 bbox_centerBigPlanet = glm::vec4((radiusBigPlanet * bbox_minSphere) + (radiusBigPlanet * bbox_maxSphere), 1.0f);
    bbox_centerBigPlanet.y = bbox_centerBigPlanet.y - 250.0f; // E movido 250 unidades para baixo no eixo y

    float distanceBigPlanet = distanceBetweenPoints(cameraPosition, bbox_centerBigPlanet);
    if( (distanceBigPlanet - radiusBigPlanet) < EPSILON ){
        glm::vec4 pushCamera = cameraPosition - bbox_centerBigPlanet; // Cria o vetor que vai empurrar a câmera para longe do planeta
        pushCamera.w = 0.0f;
        pushCamera = normalize(pushCamera) * 10.0f;
        cameraPosition = cameraPosition + pushCamera;
    }

    // ------- Planeta 2
    float radiusSmallPlanet = 10.0f; // Porque o planeta pequeno foi escalado 10 vezes
    glm::vec4 bbox_centerSmallPlanet = glm::vec4((radiusSmallPlanet * bbox_minSphere) + (radiusSmallPlanet * bbox_maxSphere), 1.0f);
    bbox_centerSmallPlanet.y = bbox_centerSmallPlanet.y + 170.0f; // E movido 170 unidades para cima no eixo y
    bbox_centerSmallPlanet.x = bbox_centerSmallPlanet.x - 245.0f; // E movido 245 unidades para esquerda no eixo x

    float distanceSmallPlanet = distanceBetweenPoints(cameraPosition, bbox_centerSmallPlanet);
    if( (distanceSmallPlanet - radiusSmallPlanet) < EPSILON ){
        glm::vec4 pushCamera = cameraPosition - bbox_centerSmallPlanet; // Cria o vetor que vai empurrar a câmera para longe do planeta
        pushCamera.w = 0.0f;
        pushCamera = normalize(pushCamera) * 3.0f;
        cameraPosition = cameraPosition + pushCamera;
    }

    // Impede que o usuário entre em qualquer um dos asteroides na cena
    glm::vec3 bbox_minAsteroid = g_VirtualScene["asteroid"].bbox_min;
    glm::vec3 bbox_maxAsteroid = g_VirtualScene["asteroid"].bbox_max;

    // ------- Asteroide 1
    if(g_AsteroidVisible[0] == true){
    glm::vec4 bbox_minFirstAsteroid = Matrix_Translate(g_AsteroidPos[0].x + g_BezierDisplacementX,
                                                       g_AsteroidPos[0].y + g_BezierDisplacementY,
                                                       g_AsteroidPos[0].z + g_BezierDisplacementZ)
                                    * Matrix_Scale(30.00f, 30.00f, 30.00f) * glm::vec4(bbox_minAsteroid, 1.0f);

    glm::vec4 bbox_maxFirstAsteroid = Matrix_Translate(g_AsteroidPos[0].x + g_BezierDisplacementX,
                                                       g_AsteroidPos[0].y + g_BezierDisplacementY,
                                                       g_AsteroidPos[0].z + g_BezierDisplacementZ)
                                    * Matrix_Scale(30.00f, 30.00f, 30.00f) * glm::vec4(bbox_maxAsteroid, 1.0f);

    if( cameraPosition.x >= bbox_minFirstAsteroid.x && cameraPosition.x <= bbox_maxFirstAsteroid.x &&
        cameraPosition.y >= bbox_minFirstAsteroid.y && cameraPosition.y <= bbox_maxFirstAsteroid.y &&
        cameraPosition.z >= bbox_minFirstAsteroid.z && cameraPosition.z <= bbox_maxFirstAsteroid.z)
    {
            glm::vec4 bbox_centerFirstAsteroid = glm::vec4((bbox_minFirstAsteroid.x + bbox_maxFirstAsteroid.x)/2.0f,
                                                           (bbox_minFirstAsteroid.y + bbox_maxFirstAsteroid.y)/2.0f,
                                                           (bbox_minFirstAsteroid.z + bbox_maxFirstAsteroid.z)/2.0f,
                                                           1.0f);

            glm::vec4 pushCamera = cameraPosition - bbox_centerFirstAsteroid; // Cria o vetor que vai empurrar a câmera para longe do asteroide
            pushCamera = normalize(pushCamera) * 10.0f;
            cameraPosition = cameraPosition + pushCamera;
    }
    }

    // ------- Asteroide 2
    if(g_AsteroidVisible[1] == true){
    glm::vec4 bbox_minSecondAsteroid = Matrix_Translate(g_AsteroidPos[1].x,g_AsteroidPos[1].y,g_AsteroidPos[1].z)
                                     * Matrix_Scale(30.00f, 30.00f, 30.00f) * glm::vec4(bbox_minAsteroid, 1.0f);

    glm::vec4 bbox_maxSecondAsteroid = Matrix_Translate(g_AsteroidPos[1].x,g_AsteroidPos[1].y,g_AsteroidPos[1].z)
                                     * Matrix_Scale(30.00f, 30.00f, 30.00f) * glm::vec4(bbox_maxAsteroid, 1.0f);

    if( cameraPosition.x >= bbox_minSecondAsteroid.x && cameraPosition.x <= bbox_maxSecondAsteroid.x &&
        cameraPosition.y >= bbox_minSecondAsteroid.y && cameraPosition.y <= bbox_maxSecondAsteroid.y &&
        cameraPosition.z >= bbox_minSecondAsteroid.z && cameraPosition.z <= bbox_maxSecondAsteroid.z)
    {
            glm::vec4 bbox_centerSecondAsteroid = glm::vec4((bbox_minSecondAsteroid.x + bbox_maxSecondAsteroid.x)/2.0f,
                                                           (bbox_minSecondAsteroid.y + bbox_maxSecondAsteroid.y)/2.0f,
                                                           (bbox_minSecondAsteroid.z + bbox_maxSecondAsteroid.z)/2.0f,
                                                           1.0f);

            glm::vec4 pushCamera = cameraPosition - bbox_centerSecondAsteroid; // Cria o vetor que vai empurrar a câmera para longe do asteroide
            pushCamera = normalize(pushCamera) * 10.0f;
            cameraPosition = cameraPosition + pushCamera;
    }
    }

    // ------- Asteroide 3
    if(g_AsteroidVisible[2] == true){
    glm::vec4 bbox_minThirdAsteroid = Matrix_Translate(g_AsteroidPos[2].x,g_AsteroidPos[2].y,g_AsteroidPos[2].z)
                                    * Matrix_Scale(35.00f, 20.00f, 35.00f) * glm::vec4(bbox_minAsteroid, 1.0f);

    glm::vec4 bbox_maxThirdAsteroid = Matrix_Translate(g_AsteroidPos[2].x,g_AsteroidPos[2].y,g_AsteroidPos[2].z)
                                    * Matrix_Scale(35.00f, 20.00f, 35.00f) * glm::vec4(bbox_maxAsteroid, 1.0f);

    if( cameraPosition.x >= bbox_minThirdAsteroid.x && cameraPosition.x <= bbox_maxThirdAsteroid.x &&
        cameraPosition.y >= bbox_minThirdAsteroid.y && cameraPosition.y <= bbox_maxThirdAsteroid.y &&
        cameraPosition.z >= bbox_minThirdAsteroid.z && cameraPosition.z <= bbox_maxThirdAsteroid.z)
    {
            glm::vec4 bbox_centerThirdAsteroid = glm::vec4((bbox_minThirdAsteroid.x + bbox_maxThirdAsteroid.x)/2.0f,
                                                           (bbox_minThirdAsteroid.y + bbox_maxThirdAsteroid.y)/2.0f,
                                                           (bbox_minThirdAsteroid.z + bbox_maxThirdAsteroid.z)/2.0f,
                                                           1.0f);

            glm::vec4 pushCamera = cameraPosition - bbox_centerThirdAsteroid; // Cria o vetor que vai empurrar a câmera para longe do asteroide
            pushCamera = normalize(pushCamera) * 10.0f;
            cameraPosition = cameraPosition + pushCamera;
    }
    }

    // ------- Asteroide 4
    if(g_AsteroidVisible[3] == true){
    glm::vec4 bbox_minFourthAsteroid = Matrix_Translate(g_AsteroidPos[3].x,g_AsteroidPos[3].y,g_AsteroidPos[3].z)
                                    * Matrix_Scale(20.00f, 20.00f, 20.00f) * glm::vec4(bbox_minAsteroid, 1.0f);

    glm::vec4 bbox_maxFourthAsteroid = Matrix_Translate(g_AsteroidPos[3].x,g_AsteroidPos[3].y,g_AsteroidPos[3].z)
                                    * Matrix_Scale(20.00f, 20.00f, 20.00f) * glm::vec4(bbox_maxAsteroid, 1.0f);

    if( cameraPosition.x >= bbox_minFourthAsteroid.x && cameraPosition.x <= bbox_maxFourthAsteroid.x &&
        cameraPosition.y >= bbox_minFourthAsteroid.y && cameraPosition.y <= bbox_maxFourthAsteroid.y &&
        cameraPosition.z >= bbox_minFourthAsteroid.z && cameraPosition.z <= bbox_maxFourthAsteroid.z)
    {
            glm::vec4 bbox_centerFourthAsteroid = glm::vec4((bbox_minFourthAsteroid.x + bbox_maxFourthAsteroid.x)/2.0f,
                                                           (bbox_minFourthAsteroid.y + bbox_maxFourthAsteroid.y)/2.0f,
                                                           (bbox_minFourthAsteroid.z + bbox_maxFourthAsteroid.z)/2.0f,
                                                           1.0f);

            glm::vec4 pushCamera = cameraPosition - bbox_centerFourthAsteroid; // Cria o vetor que vai empurrar a câmera para longe do asteroide
            pushCamera = normalize(pushCamera) * 10.0f;
            cameraPosition = cameraPosition + pushCamera;
    }
    }

    // ------- Asteroide 5
    if(g_AsteroidVisible[4] == true){
    glm::vec4 bbox_minFifthAsteroid = Matrix_Translate(g_AsteroidPos[4].x,g_AsteroidPos[4].y,g_AsteroidPos[4].z)
                                    * Matrix_Scale(25.00f, 25.00f, 25.00f) * glm::vec4(bbox_minAsteroid, 1.0f);

    glm::vec4 bbox_maxFifthAsteroid = Matrix_Translate(g_AsteroidPos[4].x,g_AsteroidPos[4].y,g_AsteroidPos[4].z)
                                    * Matrix_Scale(25.00f, 25.00f, 25.00f) * glm::vec4(bbox_maxAsteroid, 1.0f);

    if( cameraPosition.x >= bbox_minFifthAsteroid.x && cameraPosition.x <= bbox_maxFifthAsteroid.x &&
        cameraPosition.y >= bbox_minFifthAsteroid.y && cameraPosition.y <= bbox_maxFifthAsteroid.y &&
        cameraPosition.z >= bbox_minFifthAsteroid.z && cameraPosition.z <= bbox_maxFifthAsteroid.z)
    {
            glm::vec4 bbox_centerFifthAsteroid = glm::vec4((bbox_minFifthAsteroid.x + bbox_maxFifthAsteroid.x)/2.0f,
                                                           (bbox_minFifthAsteroid.y + bbox_maxFifthAsteroid.y)/2.0f,
                                                           (bbox_minFifthAsteroid.z + bbox_maxFifthAsteroid.z)/2.0f,
                                                           1.0f);

            glm::vec4 pushCamera = cameraPosition - bbox_centerFifthAsteroid; // Cria o vetor que vai empurrar a câmera para longe do asteroide
            pushCamera = normalize(pushCamera) * 10.0f;
            cameraPosition = cameraPosition + pushCamera;
    }
    }

    return cameraPosition;
}

void bulletCollision(){
// Impede que o usuário entre em qualquer um dos asteroides na cena
    glm::vec3 bbox_minAsteroid = g_VirtualScene["asteroid"].bbox_min;
    glm::vec3 bbox_maxAsteroid = g_VirtualScene["asteroid"].bbox_max;

    // ------- Asteroide 1
    if(g_AsteroidVisible[0] == true){
    glm::vec4 bbox_minFirstAsteroid = Matrix_Translate(g_AsteroidPos[0].x + g_BezierDisplacementX,
                                                       g_AsteroidPos[0].y + g_BezierDisplacementY,
                                                       g_AsteroidPos[0].z + g_BezierDisplacementZ)
                                    * Matrix_Scale(30.00f, 30.00f, 30.00f) * glm::vec4(bbox_minAsteroid, 1.0f);

    glm::vec4 bbox_maxFirstAsteroid = Matrix_Translate(g_AsteroidPos[0].x + g_BezierDisplacementX,
                                                       g_AsteroidPos[0].y + g_BezierDisplacementY,
                                                       g_AsteroidPos[0].z + g_BezierDisplacementZ)
                                    * Matrix_Scale(30.00f, 30.00f, 30.00f) * glm::vec4(bbox_maxAsteroid, 1.0f);

    if( g_BulletPosition.x >= bbox_minFirstAsteroid.x && g_BulletPosition.x <= bbox_maxFirstAsteroid.x &&
        g_BulletPosition.y >= bbox_minFirstAsteroid.y && g_BulletPosition.y <= bbox_maxFirstAsteroid.y &&
        g_BulletPosition.z >= bbox_minFirstAsteroid.z && g_BulletPosition.z <= bbox_maxFirstAsteroid.z)
        g_AsteroidVisible[0] = false;
    }

    // ------- Asteroide 2
    if(g_AsteroidVisible[1] == true){
    glm::vec4 bbox_minSecondAsteroid = Matrix_Translate(g_AsteroidPos[1].x,g_AsteroidPos[1].y,g_AsteroidPos[1].z)
                                     * Matrix_Scale(30.00f, 30.00f, 30.00f) * glm::vec4(bbox_minAsteroid, 1.0f);

    glm::vec4 bbox_maxSecondAsteroid = Matrix_Translate(g_AsteroidPos[1].x,g_AsteroidPos[1].y,g_AsteroidPos[1].z)
                                     * Matrix_Scale(30.00f, 30.00f, 30.00f) * glm::vec4(bbox_maxAsteroid, 1.0f);

    if( g_BulletPosition.x >= bbox_minSecondAsteroid.x && g_BulletPosition.x <= bbox_maxSecondAsteroid.x &&
        g_BulletPosition.y >= bbox_minSecondAsteroid.y && g_BulletPosition.y <= bbox_maxSecondAsteroid.y &&
        g_BulletPosition.z >= bbox_minSecondAsteroid.z && g_BulletPosition.z <= bbox_maxSecondAsteroid.z)
        g_AsteroidVisible[1] = false;
    }

    // ------- Asteroide 3
    if(g_AsteroidVisible[2] == true){
    glm::vec4 bbox_minThirdAsteroid = Matrix_Translate(g_AsteroidPos[2].x,g_AsteroidPos[2].y,g_AsteroidPos[2].z)
                                    * Matrix_Scale(35.00f, 20.00f, 35.00f) * glm::vec4(bbox_minAsteroid, 1.0f);

    glm::vec4 bbox_maxThirdAsteroid = Matrix_Translate(g_AsteroidPos[2].x,g_AsteroidPos[2].y,g_AsteroidPos[2].z)
                                    * Matrix_Scale(35.00f, 20.00f, 35.00f) * glm::vec4(bbox_maxAsteroid, 1.0f);

    if( g_BulletPosition.x >= bbox_minThirdAsteroid.x && g_BulletPosition.x <= bbox_maxThirdAsteroid.x &&
        g_BulletPosition.y >= bbox_minThirdAsteroid.y && g_BulletPosition.y <= bbox_maxThirdAsteroid.y &&
        g_BulletPosition.z >= bbox_minThirdAsteroid.z && g_BulletPosition.z <= bbox_maxThirdAsteroid.z)
        g_AsteroidVisible[2] = false;
    }

    // ------- Asteroide 4
    if(g_AsteroidVisible[3] == true){
    glm::vec4 bbox_minFourthAsteroid = Matrix_Translate(g_AsteroidPos[3].x,g_AsteroidPos[3].y,g_AsteroidPos[3].z)
                                    * Matrix_Scale(20.00f, 20.00f, 20.00f) * glm::vec4(bbox_minAsteroid, 1.0f);

    glm::vec4 bbox_maxFourthAsteroid = Matrix_Translate(g_AsteroidPos[3].x,g_AsteroidPos[3].y,g_AsteroidPos[3].z)
                                    * Matrix_Scale(20.00f, 20.00f, 20.00f) * glm::vec4(bbox_maxAsteroid, 1.0f);

    if( g_BulletPosition.x >= bbox_minFourthAsteroid.x && g_BulletPosition.x <= bbox_maxFourthAsteroid.x &&
        g_BulletPosition.y >= bbox_minFourthAsteroid.y && g_BulletPosition.y <= bbox_maxFourthAsteroid.y &&
        g_BulletPosition.z >= bbox_minFourthAsteroid.z && g_BulletPosition.z <= bbox_maxFourthAsteroid.z)
        g_AsteroidVisible[3] = false;
    }

    // ------- Asteroide 5
    if(g_AsteroidVisible[4] == true){
    glm::vec4 bbox_minFifthAsteroid = Matrix_Translate(g_AsteroidPos[4].x,g_AsteroidPos[4].y,g_AsteroidPos[4].z)
                                    * Matrix_Scale(25.00f, 25.00f, 25.00f) * glm::vec4(bbox_minAsteroid, 1.0f);

    glm::vec4 bbox_maxFifthAsteroid = Matrix_Translate(g_AsteroidPos[4].x,g_AsteroidPos[4].y,g_AsteroidPos[4].z)
                                    * Matrix_Scale(25.00f, 25.00f, 25.00f) * glm::vec4(bbox_maxAsteroid, 1.0f);

    if( g_BulletPosition.x >= bbox_minFifthAsteroid.x && g_BulletPosition.x <= bbox_maxFifthAsteroid.x &&
        g_BulletPosition.y >= bbox_minFifthAsteroid.y && g_BulletPosition.y <= bbox_maxFifthAsteroid.y &&
        g_BulletPosition.z >= bbox_minFifthAsteroid.z && g_BulletPosition.z <= bbox_maxFifthAsteroid.z)
        g_AsteroidVisible[4] = false;
    }
}

// Esta função recebe um vértice com coordenadas de modelo p_model e passa o
// mesmo por todos os sistemas de coordenadas armazenados nas matrizes model,
// view, e projection; e escreve na tela as matrizes e pontos resultantes
// dessas transformações.
void TextRendering_ShowModelViewProjection(
    GLFWwindow* window,
    glm::mat4 projection,
    glm::mat4 view,
    glm::mat4 model,
    glm::vec4 p_model
)
{
    if ( !g_ShowInfoText )
        return;

    glm::vec4 p_world = model*p_model;
    glm::vec4 p_camera = view*p_world;
    glm::vec4 p_clip = projection*p_camera;
    glm::vec4 p_ndc = p_clip / p_clip.w;

    float pad = TextRendering_LineHeight(window);

    TextRendering_PrintString(window, " Model matrix             Model     In World Coords.", -1.0f, 1.0f-pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, model, p_model, -1.0f, 1.0f-2*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-6*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-7*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-8*pad, 1.0f);

    TextRendering_PrintString(window, " View matrix              World     In Camera Coords.", -1.0f, 1.0f-9*pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, view, p_world, -1.0f, 1.0f-10*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-14*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-15*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-16*pad, 1.0f);

    TextRendering_PrintString(window, " Projection matrix        Camera                    In NDC", -1.0f, 1.0f-17*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductDivW(window, projection, p_camera, -1.0f, 1.0f-18*pad, 1.0f);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glm::vec2 a = glm::vec2(-1, -1);
    glm::vec2 b = glm::vec2(+1, +1);
    glm::vec2 p = glm::vec2( 0,  0);
    glm::vec2 q = glm::vec2(width, height);

    glm::mat4 viewport_mapping = Matrix(
        (q.x - p.x)/(b.x-a.x), 0.0f, 0.0f, (b.x*p.x - a.x*q.x)/(b.x-a.x),
        0.0f, (q.y - p.y)/(b.y-a.y), 0.0f, (b.y*p.y - a.y*q.y)/(b.y-a.y),
        0.0f , 0.0f , 1.0f , 0.0f ,
        0.0f , 0.0f , 0.0f , 1.0f
    );

    TextRendering_PrintString(window, "                                                       |  ", -1.0f, 1.0f-22*pad, 1.0f);
    TextRendering_PrintString(window, "                            .--------------------------'  ", -1.0f, 1.0f-23*pad, 1.0f);
    TextRendering_PrintString(window, "                            V                           ", -1.0f, 1.0f-24*pad, 1.0f);

    TextRendering_PrintString(window, " Viewport matrix           NDC      In Pixel Coords.", -1.0f, 1.0f-25*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductMoreDigits(window, viewport_mapping, p_ndc, -1.0f, 1.0f-26*pad, 1.0f);
}

// Escrevemos na tela os ângulos de Euler definidos nas variáveis globais
// g_AngleX, g_AngleY, e g_AngleZ.
void TextRendering_ShowEulerAngles(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float pad = TextRendering_LineHeight(window);

    char buffer[80];
    snprintf(buffer, 80, "Euler Angles rotation matrix = Z(%.2f)*Y(%.2f)*X(%.2f)\n", g_AngleZ, g_AngleY, g_AngleX);

    TextRendering_PrintString(window, buffer, -1.0f+pad/10, -1.0f+2*pad/10, 1.0f);
}

// Escrevemos na tela qual matriz de projeção está sendo utilizada.
void TextRendering_ShowProjection(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    if ( g_UsePerspectiveProjection )
        TextRendering_PrintString(window, "Perspective", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
    else
        TextRendering_PrintString(window, "Orthographic", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
}

// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    // Variáveis estáticas (static) mantém seus valores entre chamadas
    // subsequentes da função!
    static float old_seconds = (float)glfwGetTime();
    static int   ellapsed_frames = 0;
    static char  buffer[20] = "?? fps";
    static int   numchars = 7;

    ellapsed_frames += 1;

    // Recuperamos o número de segundos que passou desde a execução do programa
    float seconds = (float)glfwGetTime();

    // Número de segundos desde o último cálculo do fps
    float ellapsed_seconds = seconds - old_seconds;

    if ( ellapsed_seconds > 1.0f )
    {
        numchars = snprintf(buffer, 20, "%.2f fps", ellapsed_frames / ellapsed_seconds);

        old_seconds = seconds;
        ellapsed_frames = 0;
    }

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth, 1.0f-lineheight, 1.0f);
}

// Função para debugging: imprime no terminal todas informações de um modelo
// geométrico carregado de um arquivo ".obj".
// Veja: https://github.com/syoyo/tinyobjloader/blob/22883def8db9ef1f3ffb9b404318e7dd25fdbb51/loader_example.cc#L98
void PrintObjModelInfo(ObjModel* model)
{
  const tinyobj::attrib_t                & attrib    = model->attrib;
  const std::vector<tinyobj::shape_t>    & shapes    = model->shapes;
  const std::vector<tinyobj::material_t> & materials = model->materials;

  printf("# of vertices  : %d\n", (int)(attrib.vertices.size() / 3));
  printf("# of normals   : %d\n", (int)(attrib.normals.size() / 3));
  printf("# of texcoords : %d\n", (int)(attrib.texcoords.size() / 2));
  printf("# of shapes    : %d\n", (int)shapes.size());
  printf("# of materials : %d\n", (int)materials.size());

  for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
    printf("  v[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.vertices[3 * v + 0]),
           static_cast<const double>(attrib.vertices[3 * v + 1]),
           static_cast<const double>(attrib.vertices[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.normals.size() / 3; v++) {
    printf("  n[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.normals[3 * v + 0]),
           static_cast<const double>(attrib.normals[3 * v + 1]),
           static_cast<const double>(attrib.normals[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.texcoords.size() / 2; v++) {
    printf("  uv[%ld] = (%f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.texcoords[2 * v + 0]),
           static_cast<const double>(attrib.texcoords[2 * v + 1]));
  }

  // For each shape
  for (size_t i = 0; i < shapes.size(); i++) {
    printf("shape[%ld].name = %s\n", static_cast<long>(i),
           shapes[i].name.c_str());
    printf("Size of shape[%ld].indices: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.indices.size()));

    size_t index_offset = 0;

    assert(shapes[i].mesh.num_face_vertices.size() ==
           shapes[i].mesh.material_ids.size());

    printf("shape[%ld].num_faces: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.num_face_vertices.size()));

    // For each face
    for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
      size_t fnum = shapes[i].mesh.num_face_vertices[f];

      printf("  face[%ld].fnum = %ld\n", static_cast<long>(f),
             static_cast<unsigned long>(fnum));

      // For each vertex in the face
      for (size_t v = 0; v < fnum; v++) {
        tinyobj::index_t idx = shapes[i].mesh.indices[index_offset + v];
        printf("    face[%ld].v[%ld].idx = %d/%d/%d\n", static_cast<long>(f),
               static_cast<long>(v), idx.vertex_index, idx.normal_index,
               idx.texcoord_index);
      }

      printf("  face[%ld].material_id = %d\n", static_cast<long>(f),
             shapes[i].mesh.material_ids[f]);

      index_offset += fnum;
    }

    printf("shape[%ld].num_tags: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.tags.size()));
    for (size_t t = 0; t < shapes[i].mesh.tags.size(); t++) {
      printf("  tag[%ld] = %s ", static_cast<long>(t),
             shapes[i].mesh.tags[t].name.c_str());
      printf(" ints: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].intValues.size(); ++j) {
        printf("%ld", static_cast<long>(shapes[i].mesh.tags[t].intValues[j]));
        if (j < (shapes[i].mesh.tags[t].intValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" floats: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].floatValues.size(); ++j) {
        printf("%f", static_cast<const double>(
                         shapes[i].mesh.tags[t].floatValues[j]));
        if (j < (shapes[i].mesh.tags[t].floatValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" strings: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].stringValues.size(); ++j) {
        printf("%s", shapes[i].mesh.tags[t].stringValues[j].c_str());
        if (j < (shapes[i].mesh.tags[t].stringValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");
      printf("\n");
    }
  }

  for (size_t i = 0; i < materials.size(); i++) {
    printf("material[%ld].name = %s\n", static_cast<long>(i),
           materials[i].name.c_str());
    printf("  material.Ka = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].ambient[0]),
           static_cast<const double>(materials[i].ambient[1]),
           static_cast<const double>(materials[i].ambient[2]));
    printf("  material.Kd = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].diffuse[0]),
           static_cast<const double>(materials[i].diffuse[1]),
           static_cast<const double>(materials[i].diffuse[2]));
    printf("  material.Ks = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].specular[0]),
           static_cast<const double>(materials[i].specular[1]),
           static_cast<const double>(materials[i].specular[2]));
    printf("  material.Tr = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].transmittance[0]),
           static_cast<const double>(materials[i].transmittance[1]),
           static_cast<const double>(materials[i].transmittance[2]));
    printf("  material.Ke = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].emission[0]),
           static_cast<const double>(materials[i].emission[1]),
           static_cast<const double>(materials[i].emission[2]));
    printf("  material.Ns = %f\n",
           static_cast<const double>(materials[i].shininess));
    printf("  material.Ni = %f\n", static_cast<const double>(materials[i].ior));
    printf("  material.dissolve = %f\n",
           static_cast<const double>(materials[i].dissolve));
    printf("  material.illum = %d\n", materials[i].illum);
    printf("  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
    printf("  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
    printf("  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
    printf("  material.map_Ns = %s\n",
           materials[i].specular_highlight_texname.c_str());
    printf("  material.map_bump = %s\n", materials[i].bump_texname.c_str());
    printf("  material.map_d = %s\n", materials[i].alpha_texname.c_str());
    printf("  material.disp = %s\n", materials[i].displacement_texname.c_str());
    printf("  <<PBR>>\n");
    printf("  material.Pr     = %f\n", materials[i].roughness);
    printf("  material.Pm     = %f\n", materials[i].metallic);
    printf("  material.Ps     = %f\n", materials[i].sheen);
    printf("  material.Pc     = %f\n", materials[i].clearcoat_thickness);
    printf("  material.Pcr    = %f\n", materials[i].clearcoat_thickness);
    printf("  material.aniso  = %f\n", materials[i].anisotropy);
    printf("  material.anisor = %f\n", materials[i].anisotropy_rotation);
    printf("  material.map_Ke = %s\n", materials[i].emissive_texname.c_str());
    printf("  material.map_Pr = %s\n", materials[i].roughness_texname.c_str());
    printf("  material.map_Pm = %s\n", materials[i].metallic_texname.c_str());
    printf("  material.map_Ps = %s\n", materials[i].sheen_texname.c_str());
    printf("  material.norm   = %s\n", materials[i].normal_texname.c_str());
    std::map<std::string, std::string>::const_iterator it(
        materials[i].unknown_parameter.begin());
    std::map<std::string, std::string>::const_iterator itEnd(
        materials[i].unknown_parameter.end());

    for (; it != itEnd; it++) {
      printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
    }
    printf("\n");
  }
}

// set makeprg=cd\ ..\ &&\ make\ run\ >/dev/null
// vim: set spell spelllang=pt_br :

