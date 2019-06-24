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

// Cor calculada no vértice do polígono
in vec3 color_v;

// Matrizes computadas no código C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Identificador que define qual objeto está sendo desenhado no momento
#define SPHERE  0
#define SHIP 1
#define SKYBOX_BOTTOM 2
#define SKYBOX_TOP 3
#define SKYBOX_FRONT 4
#define SKYBOX_BACK 5
#define SKYBOX_LEFT 6
#define SKYBOX_RIGHT 7
#define TROPICAL 8
#define ASTEROID 9
#define BULLET 10
#define HELLO1 11
#define HELLO2 12

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
uniform sampler2D TextureImage8;
uniform sampler2D TextureImage9;
uniform sampler2D TextureImage10;
uniform sampler2D TextureImage11;

// O valor de saída ("out") de um Fragment Shader é a cor final do fragmento.
out vec3 color;

// Constantes
#define M_PI   3.14159265358979323846
#define M_PI_2 1.57079632679489661923

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
    else if (object_id == ASTEROID) {
        color = color_v;
    }
    else if (object_id == HELLO1) {
        U = texcoords.x;
        V = texcoords.y;
        color = texture(TextureImage10, vec2(U,V)).rgb;
    }
    else if (object_id == HELLO2) {
        U = texcoords.x;
        V = texcoords.y;
        color = texture(TextureImage11, vec2(U,V)).rgb;
    }
    else {

        // Obtemos a posição da câmera utilizando a inversa da matriz que define o
        // sistema de coordenadas da câmera.
        vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
        vec4 camera_position = inverse(view) * origin;
        vec4 light_position = vec4(500.0f,20.0f,50.0f, 1.0);

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
        vec4 l = normalize(light_position - p);

        // Vetor que define o sentido da câmera em relação ao ponto atual.
        vec4 v = normalize(camera_position - p);

        // Vetor que define o sentido da reflexão especular ideal.
        vec4 r = -l + 2*n*dot(n,l);

        // Vetor que define o meio do caminho entre v e l, para o modelo de iluminação Blinn-Phong
        vec4 h = (v + l)/length(v + l);

        // Parâmetros que definem as propriedades espectrais da superfície
        vec3 Kd; // Refletância difusa
        vec3 Ks; // Refletância especular
        vec3 Ka; // Refletância ambiente
        float q; // Expoente especular para o modelo de iluminação de Phong

        if ( object_id == SPHERE )
        {
            float ro = sqrt(pow(position_model.x,2.0f) + pow(position_model.y,2.0f) + pow(position_model.z,2.0f));
            float theta = atan(position_model.x, position_model.z);
            float phi = asin(position_model.y/ro);

            U = (theta + M_PI)/(2*M_PI);
            V = (phi + M_PI_2)/M_PI;

            // Propriedades espectrais do planeta
            Kd = texture(TextureImage8, vec2(U,V)).rgb;
            Ks = vec3(0.0,0.0,0.0);
            Ka = Kd;
            q = 1.0;
        }
        else if ( object_id == TROPICAL )
        {
            float ro = sqrt(pow(position_model.x,2.0f) + pow(position_model.y,2.0f) + pow(position_model.z,2.0f));
            float theta = atan(position_model.x, position_model.z);
            float phi = asin(position_model.y/ro);

            U = (theta + M_PI)/(2*M_PI);
            V = (phi + M_PI_2)/M_PI;

            // Propriedades espectrais do planeta
            Kd = texture(TextureImage9, vec2(U,V)).rgb;
            Ks = vec3(0.0,0.0,0.0);
            Ka = Kd;
            q = 1.0;
        }
        else if ( object_id == SHIP )
        {
            // Coordenadas de textura do plano, obtidas do arquivo OBJ.
            U = texcoords.x;
            V = texcoords.y;
            Kd = texture(TextureImage1, vec2(U,V)).rgb;
            Ks = Kd;
            Ka = Kd;
            q = 1.0;
        }
        else if ( object_id == BULLET )
        {
            Kd = vec3(0.8,0.8,0.8);
            Ks = vec3(0.8,0.8,0.8);
            Ka = vec3(0.8,0.8,0.8);
            q = 20.0;
        }
        else // Objeto desconhecido = branco
        {
            Kd = vec3(1.0,1.0,1.0);
            Ks = vec3(1.0,1.0,1.0);
            Ka = vec3(1.0,1.0,1.0);
            q = 1.0;
        }

        // Espectro da fonte de iluminação
        vec3 I = vec3(0.5,1.0,0.5);

        // Espectro da luz ambiente
        vec3 Ia = vec3(0.4,0.4,0.4);

        // Distância da fonte de luz para o fragmento
        float dist = distance(light_position, p);

        // Fator atenuante da fonte de luz, de acordo com sua distância para o fragmento
        //float att = 1.0/(1.0 + 0.0001*dist + 0.00001*dist*dist);
        float att = 1.0;

        // Termo difuso utilizando a lei dos cossenos de Lambert
        vec3 lambert_diffuse_term = att * Kd * I * max(0, dot(n,l));

        // Termo ambiente
        vec3 ambient_term = Ka * Ia;

        // Termo especular utilizando o modelo de iluminação de Phong
        //vec3 phong_specular_term = Ks * I * max(0, pow(dot(r,v),q));

        // Termo especular utilizando o modelo de iluminação de Blinn-Phong
        vec3 phong_specular_term = att * Ks * I * max(0, pow(dot(n,h),q));

        // Cor final do fragmento calculada com uma combinação dos termos difuso,
        // especular, e ambiente. Veja slide 133 do documento "Aula_17_e_18_Modelos_de_Iluminacao.pdf".
        color = lambert_diffuse_term + ambient_term + phong_specular_term;
    }

    // Cor final com correção gamma, considerando monitor sRGB.
    // Veja https://en.wikipedia.org/w/index.php?title=Gamma_correction&oldid=751281772#Windows.2C_Mac.2C_sRGB_and_TV.2Fvideo_standard_gammas
    color = pow(color, vec3(1.0,1.0,1.0)/2.2);
}

