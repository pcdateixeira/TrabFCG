#version 330 core

// Atributos de fragmentos recebidos como entrada ("in") pelo Fragment Shader.
// Neste exemplo, este atributo foi gerado pelo rasterizador como a
// interpolação da posição global e a normal de cada vértice, definidas em
// "shader_vertex.glsl" e "main.cpp".
in vec4 position_world;
in vec4 normal;

// Posição do vértice atual no sistema de coordenadas local do modelo.
in vec4 position_model;

// Coordenadas de textura obtidas do arquivo OBJ (se existirem!)
in vec2 texcoords;

// Matrizes computadas no código C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Identificador que define qual objeto está sendo desenhado no momento
#define BUNNY  0
#define SHIP 1
#define SKYBOX_BOTTOM 2
#define SKYBOX_TOP 3
#define SKYBOX_FRONT 4
#define SKYBOX_BACK 5
#define SKYBOX_LEFT 6
#define SKYBOX_RIGHT 7

uniform int object_id;

// Parâmetros da axis-aligned bounding box (AABB) do modelo
uniform vec4 bbox_min;
uniform vec4 bbox_max;

// Variáveis para acesso das imagens de textura
uniform sampler2D TextureImage0;
uniform sampler2D TextureImage1;
uniform sampler2D TextureImage2;
uniform sampler2D TextureImage3;
uniform sampler2D TextureImage4;
uniform sampler2D TextureImage5;
uniform sampler2D TextureImage6;
uniform sampler2D TextureImage7;

// O valor de saída ("out") de um Fragment Shader é a cor final do fragmento.
out vec3 color;

void main()
{
    // Coordenadas de textura U e V
    float U = 0.0;
    float V = 0.0;

    // Carrega as texturas da skybox, que não são afetadas por iluminação
    if ( object_id >= SKYBOX_BOTTOM && object_id <= SKYBOX_RIGHT ) {
        U = texcoords.x;
        V = texcoords.y;

        switch(object_id) {
        case SKYBOX_BOTTOM:
            color = texture(TextureImage2, vec2(U,V)).rgb;
            break;
        case SKYBOX_TOP:
            color = texture(TextureImage3, vec2(U,V)).rgb;
            break;
        case SKYBOX_FRONT:
            color = texture(TextureImage4, vec2(U,V)).rgb;
            break;
        case SKYBOX_BACK:
            color = texture(TextureImage5, vec2(U,V)).rgb;
            break;
        case SKYBOX_LEFT:
            color = texture(TextureImage6, vec2(U,V)).rgb;
            break;
        case SKYBOX_RIGHT:
            color = texture(TextureImage7, vec2(U,V)).rgb;
        }
    }
    else {

        // Obtemos a posição da câmera utilizando a inversa da matriz que define o
        // sistema de coordenadas da câmera.
        vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
        vec4 camera_position = inverse(view) * origin;

        // O fragmento atual é coberto por um ponto que percente à superfície de um
        // dos objetos virtuais da cena. Este ponto, p, possui uma posição no
        // sistema de coordenadas global (World coordinates). Esta posição é obtida
        // através da interpolação, feita pelo rasterizador, da posição de cada
        // vértice.
        vec4 p = position_world;

        // Normal do fragmento atual, interpolada pelo rasterizador a partir das
        // normais de cada vértice.
        vec4 n = normalize(normal);

        // Vetor que define o sentido da fonte de luz em relação ao ponto atual.
        vec4 l = normalize(camera_position - p);

        // Vetor que define o sentido da câmera em relação ao ponto atual.
        vec4 v = normalize(camera_position - p);

        // Vetor que define o sentido da reflexão especular ideal.
        vec4 r = -l + 2*n*dot(n,l);

        // Parâmetros que definem as propriedades espectrais da superfície
        vec3 Kd; // Refletância difusa
        vec3 Ks; // Refletância especular
        vec3 Ka; // Refletância ambiente
        float q; // Expoente especular para o modelo de iluminação de Phong

        if ( object_id == BUNNY )
        {
            // Propriedades espectrais do coelho
            Kd = vec3(0.08,0.4,0.8);
            Ks = vec3(0.8,0.8,0.8);
            Ka = Kd/2;
            q = 32.0;
        }
        else if ( object_id == SHIP )
        {
            // Coordenadas de textura do plano, obtidas do arquivo OBJ.
            U = texcoords.x;
            V = texcoords.y;
            Kd = texture(TextureImage1, vec2(U,V)).rgb;
        }
        else // Objeto desconhecido = preto
        {
            Kd = vec3(0.0,0.0,0.0);
            Ks = vec3(0.0,0.0,0.0);
            Ka = vec3(0.0,0.0,0.0);
            q = 1.0;
        }

        // Espectro da fonte de iluminação
        vec3 I = vec3(1.0,1.0,1.0);

        // Espectro da luz ambiente
        vec3 Ia = vec3(0.2,0.2,0.2);

        // Termo difuso utilizando a lei dos cossenos de Lambert
        vec3 lambert_diffuse_term = Kd * I * max(0, dot(n,l));

        // Termo ambiente
        vec3 ambient_term = Ka * Ia;

        // Termo especular utilizando o modelo de iluminação de Phong
        vec3 phong_specular_term = Ks * I * max(0, pow(dot(r,v),q));

        // Cor final do fragmento calculada com uma combinação dos termos difuso,
        // especular, e ambiente. Veja slide 133 do documento "Aula_17_e_18_Modelos_de_Iluminacao.pdf".
        if ( object_id == SHIP ){
            color = lambert_diffuse_term;
        }
        else {
            color = lambert_diffuse_term + ambient_term + phong_specular_term;
        }
    }

    // Cor final com correção gamma, considerando monitor sRGB.
    // Veja https://en.wikipedia.org/w/index.php?title=Gamma_correction&oldid=751281772#Windows.2C_Mac.2C_sRGB_and_TV.2Fvideo_standard_gammas
    color = pow(color, vec3(1.0,1.0,1.0)/2.2);
}

