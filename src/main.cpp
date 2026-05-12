#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl2.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

static Vec3 operator+(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vec3 operator-(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vec3 operator*(Vec3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

static Vec3& operator+=(Vec3& a, Vec3 b) {
    a = a + b;
    return a;
}

struct Color {
    float r = 0.18f;
    float g = 0.58f;
    float b = 0.92f;
};

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    Vec3 boundsMin = {-0.5f, -0.5f, -0.5f};
    Vec3 boundsMax = {0.5f, 0.5f, 0.5f};
};

enum class MeshType {
    Cube = 0,
    Plane = 1,
    Sphere = 2,
    Cylinder = 3,
    Imported = 4
};

enum class EditSelectionMode {
    Vertex = 0,
    Edge = 1,
    Face = 2
};

struct Material {
    Color baseColor;
    bool useCheckerTexture = false;
    bool useImageTexture = false;
    std::string texturePath;
    unsigned int textureId = 0;
};

struct SceneObject {
    std::string name;
    MeshType meshType = MeshType::Cube;
    Mesh mesh;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
    Material material;
};

struct Camera {
    float yaw = 45.0f;
    float pitch = 28.0f;
    float distance = 7.0f;
    Vec3 target = {0.0f, 0.6f, 0.0f};
};

struct Light {
    Vec3 direction = {-0.35f, -0.8f, -0.45f};
    Color color = {1.0f, 0.96f, 0.88f};
    float intensity = 0.9f;
    float ambient = 0.28f;
};

struct AppState {
    Camera camera;
    Light light;
    std::array<Mesh, 4> meshes;
    std::vector<SceneObject> objects;
    int selectedIndex = 0;
    bool showDemoWindow = false;
    bool showGrid = true;
    bool showWireframe = true;
    bool lightingEnabled = true;
    bool editMode = false;
    EditSelectionMode editSelectionMode = EditSelectionMode::Vertex;
    int selectedVertex = -1;
    int selectedEdgeA = -1;
    int selectedEdgeB = -1;
    int selectedTriangle = -1;
    bool leftDragging = false;
    bool rightDragging = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    double mouseDownX = 0.0;
    double mouseDownY = 0.0;
    char scenePath[260] = "scene.json";
    char importPath[260] = "model.obj";
    char texturePath[260] = "texture.bmp";
    std::string status = "Ready";
    unsigned int checkerTexture = 0;
};

static AppState gApp;

static float radians(float degrees) {
    return degrees * 3.1415926535f / 180.0f;
}

static float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static float length(Vec3 value) {
    return std::sqrt(dot(value, value));
}

static Vec3 normalize(Vec3 value) {
    const float valueLength = length(value);
    if (valueLength <= 0.00001f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return value * (1.0f / valueLength);
}

static Vec3 rotateX(Vec3 value, float degrees) {
    const float c = std::cos(radians(degrees));
    const float s = std::sin(radians(degrees));
    return {value.x, value.y * c - value.z * s, value.y * s + value.z * c};
}

static Vec3 rotateY(Vec3 value, float degrees) {
    const float c = std::cos(radians(degrees));
    const float s = std::sin(radians(degrees));
    return {value.x * c + value.z * s, value.y, -value.x * s + value.z * c};
}

static Vec3 rotateZ(Vec3 value, float degrees) {
    const float c = std::cos(radians(degrees));
    const float s = std::sin(radians(degrees));
    return {value.x * c - value.y * s, value.x * s + value.y * c, value.z};
}

static const char* meshTypeName(MeshType type) {
    switch (type) {
        case MeshType::Cube: return "Cube";
        case MeshType::Plane: return "Plane";
        case MeshType::Sphere: return "Sphere";
        case MeshType::Cylinder: return "Cylinder";
        case MeshType::Imported: return "Imported";
    }
    return "Cube";
}

static MeshType meshTypeFromName(const std::string& name) {
    if (name == "Plane") {
        return MeshType::Plane;
    }
    if (name == "Sphere") {
        return MeshType::Sphere;
    }
    if (name == "Cylinder") {
        return MeshType::Cylinder;
    }
    if (name == "Imported") {
        return MeshType::Imported;
    }
    return MeshType::Cube;
}

static int meshIndex(MeshType type) {
    return static_cast<int>(type);
}

static void addQuad(Mesh& mesh, Vertex a, Vertex b, Vertex c, Vertex d) {
    const unsigned int start = static_cast<unsigned int>(mesh.vertices.size());
    mesh.vertices.push_back(a);
    mesh.vertices.push_back(b);
    mesh.vertices.push_back(c);
    mesh.vertices.push_back(d);
    mesh.indices.insert(mesh.indices.end(), {start, start + 1, start + 2, start, start + 2, start + 3});
}

static Mesh makeCubeMesh() {
    Mesh mesh;
    addQuad(mesh, {{-0.5f, -0.5f,  0.5f}, {0, 0, 1}, {0, 0}}, {{ 0.5f, -0.5f,  0.5f}, {0, 0, 1}, {1, 0}}, {{ 0.5f,  0.5f,  0.5f}, {0, 0, 1}, {1, 1}}, {{-0.5f,  0.5f,  0.5f}, {0, 0, 1}, {0, 1}});
    addQuad(mesh, {{ 0.5f, -0.5f,  0.5f}, {1, 0, 0}, {0, 0}}, {{ 0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 0}}, {{ 0.5f,  0.5f, -0.5f}, {1, 0, 0}, {1, 1}}, {{ 0.5f,  0.5f,  0.5f}, {1, 0, 0}, {0, 1}});
    addQuad(mesh, {{ 0.5f, -0.5f, -0.5f}, {0, 0,-1}, {0, 0}}, {{-0.5f, -0.5f, -0.5f}, {0, 0,-1}, {1, 0}}, {{-0.5f,  0.5f, -0.5f}, {0, 0,-1}, {1, 1}}, {{ 0.5f,  0.5f, -0.5f}, {0, 0,-1}, {0, 1}});
    addQuad(mesh, {{-0.5f, -0.5f, -0.5f}, {-1,0, 0}, {0, 0}}, {{-0.5f, -0.5f,  0.5f}, {-1,0, 0}, {1, 0}}, {{-0.5f,  0.5f,  0.5f}, {-1,0, 0}, {1, 1}}, {{-0.5f,  0.5f, -0.5f}, {-1,0, 0}, {0, 1}});
    addQuad(mesh, {{-0.5f,  0.5f,  0.5f}, {0, 1, 0}, {0, 0}}, {{ 0.5f,  0.5f,  0.5f}, {0, 1, 0}, {1, 0}}, {{ 0.5f,  0.5f, -0.5f}, {0, 1, 0}, {1, 1}}, {{-0.5f,  0.5f, -0.5f}, {0, 1, 0}, {0, 1}});
    addQuad(mesh, {{-0.5f, -0.5f, -0.5f}, {0,-1, 0}, {0, 0}}, {{ 0.5f, -0.5f, -0.5f}, {0,-1, 0}, {1, 0}}, {{ 0.5f, -0.5f,  0.5f}, {0,-1, 0}, {1, 1}}, {{-0.5f, -0.5f,  0.5f}, {0,-1, 0}, {0, 1}});
    return mesh;
}

static Mesh makePlaneMesh() {
    Mesh mesh;
    mesh.boundsMin = {-0.5f, -0.01f, -0.5f};
    mesh.boundsMax = {0.5f, 0.01f, 0.5f};
    addQuad(mesh, {{-0.5f, 0.0f,  0.5f}, {0, 1, 0}, {0, 0}}, {{ 0.5f, 0.0f,  0.5f}, {0, 1, 0}, {1, 0}}, {{ 0.5f, 0.0f, -0.5f}, {0, 1, 0}, {1, 1}}, {{-0.5f, 0.0f, -0.5f}, {0, 1, 0}, {0, 1}});
    return mesh;
}

static Mesh makeSphereMesh(int stacks = 16, int slices = 32) {
    Mesh mesh;
    for (int stack = 0; stack <= stacks; ++stack) {
        const float v = static_cast<float>(stack) / static_cast<float>(stacks);
        const float phi = v * 3.1415926535f;
        for (int slice = 0; slice <= slices; ++slice) {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float theta = u * 2.0f * 3.1415926535f;
            Vec3 normal = {
                std::sin(phi) * std::cos(theta),
                std::cos(phi),
                std::sin(phi) * std::sin(theta)
            };
            mesh.vertices.push_back({normal * 0.5f, normalize(normal), {u, v}});
        }
    }

    for (int stack = 0; stack < stacks; ++stack) {
        for (int slice = 0; slice < slices; ++slice) {
            const unsigned int a = static_cast<unsigned int>(stack * (slices + 1) + slice);
            const unsigned int b = a + static_cast<unsigned int>(slices + 1);
            mesh.indices.insert(mesh.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    return mesh;
}

static Mesh makeCylinderMesh(int segments = 32) {
    Mesh mesh;
    const unsigned int topCenter = static_cast<unsigned int>(mesh.vertices.size());
    mesh.vertices.push_back({{0, 0.5f, 0}, {0, 1, 0}, {0.5f, 0.5f}});
    const unsigned int bottomCenter = static_cast<unsigned int>(mesh.vertices.size());
    mesh.vertices.push_back({{0, -0.5f, 0}, {0, -1, 0}, {0.5f, 0.5f}});

    for (int segment = 0; segment <= segments; ++segment) {
        const float u = static_cast<float>(segment) / static_cast<float>(segments);
        const float theta = u * 2.0f * 3.1415926535f;
        const float x = std::cos(theta) * 0.5f;
        const float z = std::sin(theta) * 0.5f;
        const Vec3 sideNormal = normalize({x, 0.0f, z});
        mesh.vertices.push_back({{x, 0.5f, z}, sideNormal, {u, 1.0f}});
        mesh.vertices.push_back({{x, -0.5f, z}, sideNormal, {u, 0.0f}});
        mesh.vertices.push_back({{x, 0.5f, z}, {0, 1, 0}, {x + 0.5f, z + 0.5f}});
        mesh.vertices.push_back({{x, -0.5f, z}, {0, -1, 0}, {x + 0.5f, z + 0.5f}});
    }

    for (int segment = 0; segment < segments; ++segment) {
        const unsigned int base = 2 + static_cast<unsigned int>(segment) * 4;
        const unsigned int next = base + 4;
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, next, next, base + 1, next + 1});
        mesh.indices.insert(mesh.indices.end(), {topCenter, next + 2, base + 2});
        mesh.indices.insert(mesh.indices.end(), {bottomCenter, base + 3, next + 3});
    }
    return mesh;
}

static void buildMeshes() {
    gApp.meshes[meshIndex(MeshType::Cube)] = makeCubeMesh();
    gApp.meshes[meshIndex(MeshType::Plane)] = makePlaneMesh();
    gApp.meshes[meshIndex(MeshType::Sphere)] = makeSphereMesh();
    gApp.meshes[meshIndex(MeshType::Cylinder)] = makeCylinderMesh();
}

static bool isPrimitiveMesh(MeshType type) {
    return type != MeshType::Imported;
}

static void recalculateMesh(Mesh& mesh) {
    if (mesh.vertices.empty()) {
        mesh.boundsMin = {-0.5f, -0.5f, -0.5f};
        mesh.boundsMax = {0.5f, 0.5f, 0.5f};
        return;
    }

    mesh.boundsMin = mesh.vertices.front().position;
    mesh.boundsMax = mesh.vertices.front().position;
    for (Vertex& vertex : mesh.vertices) {
        vertex.normal = {0.0f, 0.0f, 0.0f};
        mesh.boundsMin.x = std::min(mesh.boundsMin.x, vertex.position.x);
        mesh.boundsMin.y = std::min(mesh.boundsMin.y, vertex.position.y);
        mesh.boundsMin.z = std::min(mesh.boundsMin.z, vertex.position.z);
        mesh.boundsMax.x = std::max(mesh.boundsMax.x, vertex.position.x);
        mesh.boundsMax.y = std::max(mesh.boundsMax.y, vertex.position.y);
        mesh.boundsMax.z = std::max(mesh.boundsMax.z, vertex.position.z);
    }

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        Vertex& a = mesh.vertices[mesh.indices[i]];
        Vertex& b = mesh.vertices[mesh.indices[i + 1]];
        Vertex& c = mesh.vertices[mesh.indices[i + 2]];
        const Vec3 normal = normalize(cross(b.position - a.position, c.position - a.position));
        a.normal += normal;
        b.normal += normal;
        c.normal += normal;
    }

    for (Vertex& vertex : mesh.vertices) {
        vertex.normal = normalize(vertex.normal);
    }
}

static void clearEditSelection() {
    gApp.selectedVertex = -1;
    gApp.selectedEdgeA = -1;
    gApp.selectedEdgeB = -1;
    gApp.selectedTriangle = -1;
}

static Vec3 cameraPosition() {
    const Camera& camera = gApp.camera;
    const float yaw = radians(camera.yaw);
    const float pitch = radians(camera.pitch);
    return {
        camera.target.x + camera.distance * std::cos(pitch) * std::sin(yaw),
        camera.target.y + camera.distance * std::sin(pitch),
        camera.target.z + camera.distance * std::cos(pitch) * std::cos(yaw)
    };
}

static void cameraBasis(Vec3& eye, Vec3& forward, Vec3& right, Vec3& up) {
    eye = cameraPosition();
    forward = normalize(gApp.camera.target - eye);
    right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
    up = normalize(cross(right, forward));
}

static void addPrimitive(MeshType type) {
    const int index = static_cast<int>(gApp.objects.size());
    const Color colors[] = {
        {0.18f, 0.58f, 0.92f},
        {0.95f, 0.42f, 0.22f},
        {0.36f, 0.78f, 0.45f},
        {0.86f, 0.67f, 0.19f}
    };

    SceneObject object;
    object.meshType = type;
    object.mesh = gApp.meshes[meshIndex(type)];
    object.name = std::string(meshTypeName(type)) + " " + std::to_string(index + 1);
    object.position = {
        (static_cast<float>(index % 5) - 2.0f) * 1.5f,
        type == MeshType::Plane ? 0.02f : 0.6f,
        -std::floor(static_cast<float>(index) / 5.0f) * 1.5f
    };
    object.rotation = {0.0f, type == MeshType::Sphere ? 0.0f : 20.0f * static_cast<float>(index), 0.0f};
    object.scale = type == MeshType::Plane ? Vec3{2.0f, 1.0f, 2.0f} : Vec3{1.0f, 1.0f, 1.0f};
    object.material.baseColor = colors[index % 4];
    object.material.useCheckerTexture = type == MeshType::Plane;

    gApp.objects.push_back(object);
    gApp.selectedIndex = static_cast<int>(gApp.objects.size()) - 1;
    clearEditSelection();
}

static void deleteSelectedObject() {
    if (gApp.objects.empty()) {
        return;
    }

    gApp.objects.erase(gApp.objects.begin() + gApp.selectedIndex);
    gApp.selectedIndex = std::clamp(gApp.selectedIndex, 0, std::max(0, static_cast<int>(gApp.objects.size()) - 1));
    clearEditSelection();
}

static void resetCamera() {
    gApp.camera = Camera{};
}

static bool loadBmpTexture(const std::string& path, unsigned int& textureId) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        gApp.status = "Failed to open texture " + path;
        return false;
    }

    unsigned char header[54] = {};
    input.read(reinterpret_cast<char*>(header), sizeof(header));
    if (!input || header[0] != 'B' || header[1] != 'M') {
        gApp.status = "Texture must be a 24-bit BMP file";
        return false;
    }

    const int dataOffset = *reinterpret_cast<int*>(&header[10]);
    const int width = *reinterpret_cast<int*>(&header[18]);
    const int rawHeight = *reinterpret_cast<int*>(&header[22]);
    const short bitsPerPixel = *reinterpret_cast<short*>(&header[28]);
    const int compression = *reinterpret_cast<int*>(&header[30]);
    if (width <= 0 || rawHeight == 0 || bitsPerPixel != 24 || compression != 0) {
        gApp.status = "Only uncompressed 24-bit BMP textures are supported";
        return false;
    }

    const int height = std::abs(rawHeight);
    const int rowStride = ((width * 3 + 3) / 4) * 4;
    std::vector<unsigned char> row(static_cast<size_t>(rowStride));
    std::vector<unsigned char> pixels(static_cast<size_t>(width * height * 3));

    input.seekg(dataOffset, std::ios::beg);
    for (int y = 0; y < height; ++y) {
        input.read(reinterpret_cast<char*>(row.data()), rowStride);
        const int targetY = rawHeight > 0 ? (height - 1 - y) : y;
        for (int x = 0; x < width; ++x) {
            const size_t source = static_cast<size_t>(x * 3);
            const size_t target = static_cast<size_t>((targetY * width + x) * 3);
            pixels[target + 0] = row[source + 2];
            pixels[target + 1] = row[source + 1];
            pixels[target + 2] = row[source + 0];
        }
    }

    if (textureId == 0) {
        glGenTextures(1, &textureId);
    }
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    gApp.status = "Loaded texture " + path;
    return true;
}

static bool parseObjFaceToken(const std::string& token, int& positionIndex, int& uvIndex, int& normalIndex) {
    positionIndex = 0;
    uvIndex = 0;
    normalIndex = 0;

    const size_t firstSlash = token.find('/');
    if (firstSlash == std::string::npos) {
        positionIndex = std::atoi(token.c_str());
        return positionIndex != 0;
    }

    positionIndex = std::atoi(token.substr(0, firstSlash).c_str());
    const size_t secondSlash = token.find('/', firstSlash + 1);
    if (secondSlash == std::string::npos) {
        uvIndex = std::atoi(token.substr(firstSlash + 1).c_str());
    } else {
        const std::string uvText = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
        if (!uvText.empty()) {
            uvIndex = std::atoi(uvText.c_str());
        }
        normalIndex = std::atoi(token.substr(secondSlash + 1).c_str());
    }

    return positionIndex != 0;
}

template <typename T>
static const T& objAt(const std::vector<T>& values, int oneBasedIndex, const T& fallback) {
    if (oneBasedIndex > 0 && oneBasedIndex <= static_cast<int>(values.size())) {
        return values[static_cast<size_t>(oneBasedIndex - 1)];
    }
    return fallback;
}

static bool importObj(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        gApp.status = "Failed to open OBJ " + path;
        return false;
    }

    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
    std::vector<Vec3> normals;
    Mesh mesh;
    const Vec3 fallbackPosition = {0.0f, 0.0f, 0.0f};
    const Vec2 fallbackUv = {0.0f, 0.0f};
    const Vec3 fallbackNormal = {0.0f, 1.0f, 0.0f};

    std::string line;
    while (std::getline(input, line)) {
        std::stringstream stream(line);
        std::string tag;
        stream >> tag;
        if (tag == "v") {
            Vec3 position;
            stream >> position.x >> position.y >> position.z;
            positions.push_back(position);
        } else if (tag == "vt") {
            Vec2 uv;
            stream >> uv.x >> uv.y;
            uvs.push_back(uv);
        } else if (tag == "vn") {
            Vec3 normal;
            stream >> normal.x >> normal.y >> normal.z;
            normals.push_back(normalize(normal));
        } else if (tag == "f") {
            std::vector<unsigned int> faceIndices;
            std::string token;
            while (stream >> token) {
                int positionIndex = 0;
                int uvIndex = 0;
                int normalIndex = 0;
                if (!parseObjFaceToken(token, positionIndex, uvIndex, normalIndex)) {
                    continue;
                }

                Vertex vertex;
                vertex.position = objAt(positions, positionIndex, fallbackPosition);
                vertex.uv = objAt(uvs, uvIndex, fallbackUv);
                vertex.normal = objAt(normals, normalIndex, fallbackNormal);
                faceIndices.push_back(static_cast<unsigned int>(mesh.vertices.size()));
                mesh.vertices.push_back(vertex);
            }

            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                mesh.indices.insert(mesh.indices.end(), {faceIndices[0], faceIndices[i], faceIndices[i + 1]});
            }
        }
    }

    if (mesh.vertices.empty() || mesh.indices.empty()) {
        gApp.status = "OBJ has no supported mesh data";
        return false;
    }

    recalculateMesh(mesh);
    SceneObject object;
    object.meshType = MeshType::Imported;
    object.mesh = mesh;
    object.name = "Imported OBJ " + std::to_string(gApp.objects.size() + 1);
    object.position = {0.0f, 0.6f, 0.0f};
    object.material.baseColor = {0.78f, 0.78f, 0.72f};
    gApp.objects.push_back(object);
    gApp.selectedIndex = static_cast<int>(gApp.objects.size()) - 1;
    clearEditSelection();
    gApp.status = "Imported OBJ " + path;
    return true;
}

static std::string escapeJson(const std::string& text) {
    std::string escaped;
    for (char character : text) {
        if (character == '"' || character == '\\') {
            escaped += '\\';
        }
        escaped += character;
    }
    return escaped;
}

static void saveScene() {
    std::ofstream output(gApp.scenePath);
    if (!output) {
        gApp.status = "Failed to save " + std::string(gApp.scenePath);
        return;
    }

    output << "{\n";
    output << "  \"camera\": {\n";
    output << "    \"yaw\": " << gApp.camera.yaw << ",\n";
    output << "    \"pitch\": " << gApp.camera.pitch << ",\n";
    output << "    \"distance\": " << gApp.camera.distance << ",\n";
    output << "    \"target\": [" << gApp.camera.target.x << ", " << gApp.camera.target.y << ", " << gApp.camera.target.z << "]\n";
    output << "  },\n";
    output << "  \"light\": {\n";
    output << "    \"direction\": [" << gApp.light.direction.x << ", " << gApp.light.direction.y << ", " << gApp.light.direction.z << "],\n";
    output << "    \"color\": [" << gApp.light.color.r << ", " << gApp.light.color.g << ", " << gApp.light.color.b << "],\n";
    output << "    \"intensity\": " << gApp.light.intensity << ",\n";
    output << "    \"ambient\": " << gApp.light.ambient << "\n";
    output << "  },\n";
    output << "  \"objects\": [\n";

    for (size_t i = 0; i < gApp.objects.size(); ++i) {
        const SceneObject& object = gApp.objects[i];
        output << "    {\n";
        output << "      \"name\": \"" << escapeJson(object.name) << "\",\n";
        output << "      \"mesh\": \"" << meshTypeName(object.meshType) << "\",\n";
        output << "      \"position\": [" << object.position.x << ", " << object.position.y << ", " << object.position.z << "],\n";
        output << "      \"rotation\": [" << object.rotation.x << ", " << object.rotation.y << ", " << object.rotation.z << "],\n";
        output << "      \"scale\": [" << object.scale.x << ", " << object.scale.y << ", " << object.scale.z << "],\n";
        output << "      \"color\": [" << object.material.baseColor.r << ", " << object.material.baseColor.g << ", " << object.material.baseColor.b << "],\n";
        output << "      \"checkerTexture\": " << (object.material.useCheckerTexture ? "true" : "false") << ",\n";
        output << "      \"imageTexture\": " << (object.material.useImageTexture ? "true" : "false") << ",\n";
        output << "      \"texturePath\": \"" << escapeJson(object.material.texturePath) << "\",\n";
        output << "      \"vertices\": [";
        for (size_t vertexIndex = 0; vertexIndex < object.mesh.vertices.size(); ++vertexIndex) {
            const Vec3& position = object.mesh.vertices[vertexIndex].position;
            output << "[" << position.x << ", " << position.y << ", " << position.z << "]";
            if (vertexIndex + 1 != object.mesh.vertices.size()) {
                output << ", ";
            }
        }
        output << "],\n";
        output << "      \"indices\": [";
        for (size_t index = 0; index < object.mesh.indices.size(); ++index) {
            output << object.mesh.indices[index];
            if (index + 1 != object.mesh.indices.size()) {
                output << ", ";
            }
        }
        output << "]\n";
        output << "    }" << (i + 1 == gApp.objects.size() ? "\n" : ",\n");
    }

    output << "  ]\n";
    output << "}\n";
    gApp.status = "Saved " + std::string(gApp.scenePath);
}

class JsonCursor {
public:
    explicit JsonCursor(std::string text) : text_(std::move(text)) {}

    bool findKey(const std::string& key) {
        const std::string quoted = "\"" + key + "\"";
        const size_t found = text_.find(quoted, position_);
        if (found == std::string::npos) {
            return false;
        }
        position_ = found + quoted.size();
        return true;
    }

    bool readString(std::string& value) {
        if (!findNext('"')) {
            return false;
        }

        std::string result;
        while (position_ < text_.size()) {
            const char character = text_[position_++];
            if (character == '"') {
                value = result;
                return true;
            }
            if (character == '\\' && position_ < text_.size()) {
                result += text_[position_++];
            } else {
                result += character;
            }
        }
        return false;
    }

    bool readFloat(float& value) {
        skipToNumberOrBool();
        if (position_ >= text_.size()) {
            return false;
        }

        char* end = nullptr;
        value = std::strtof(text_.c_str() + position_, &end);
        if (end == text_.c_str() + position_) {
            return false;
        }
        position_ = static_cast<size_t>(end - text_.c_str());
        return true;
    }

    bool readBool(bool& value) {
        skipToNumberOrBool();
        if (text_.compare(position_, 4, "true") == 0) {
            value = true;
            position_ += 4;
            return true;
        }
        if (text_.compare(position_, 5, "false") == 0) {
            value = false;
            position_ += 5;
            return true;
        }
        return false;
    }

    bool readVec3(Vec3& value) {
        if (!findNext('[')) {
            return false;
        }
        return readFloat(value.x) && readFloat(value.y) && readFloat(value.z) && findNext(']');
    }

    bool readColor(Color& value) {
        Vec3 color;
        if (!readVec3(color)) {
            return false;
        }
        value = {color.x, color.y, color.z};
        return true;
    }

    bool readVec3Array(std::vector<Vec3>& values) {
        values.clear();
        if (!findNext('[')) {
            return false;
        }

        while (position_ < text_.size()) {
            while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_]))) {
                ++position_;
            }
            if (position_ < text_.size() && text_[position_] == ']') {
                ++position_;
                return true;
            }

            Vec3 value;
            if (!readVec3(value)) {
                return false;
            }
            values.push_back(value);

            while (position_ < text_.size() && text_[position_] != '[' && text_[position_] != ']') {
                ++position_;
            }
        }
        return false;
    }

    bool readUIntArray(std::vector<unsigned int>& values) {
        values.clear();
        if (!findNext('[')) {
            return false;
        }

        while (position_ < text_.size()) {
            while (position_ < text_.size() && (std::isspace(static_cast<unsigned char>(text_[position_])) || text_[position_] == ',')) {
                ++position_;
            }
            if (position_ < text_.size() && text_[position_] == ']') {
                ++position_;
                return true;
            }
            if (position_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[position_]))) {
                return false;
            }

            char* end = nullptr;
            const unsigned long value = std::strtoul(text_.c_str() + position_, &end, 10);
            if (end == text_.c_str() + position_) {
                return false;
            }
            values.push_back(static_cast<unsigned int>(value));
            position_ = static_cast<size_t>(end - text_.c_str());
        }
        return false;
    }

    std::string readObjectBlock() {
        if (!findNext('{')) {
            return {};
        }

        const size_t begin = position_ - 1;
        int depth = 1;
        while (position_ < text_.size() && depth > 0) {
            const char character = text_[position_++];
            if (character == '{') {
                ++depth;
            } else if (character == '}') {
                --depth;
            }
        }

        if (depth != 0) {
            return {};
        }
        return text_.substr(begin, position_ - begin);
    }

private:
    bool findNext(char target) {
        const size_t found = text_.find(target, position_);
        if (found == std::string::npos) {
            return false;
        }
        position_ = found + 1;
        return true;
    }

    void skipToNumberOrBool() {
        while (position_ < text_.size()) {
            const char character = text_[position_];
            if (std::isdigit(static_cast<unsigned char>(character)) || character == '-' || character == '+' || character == 't' || character == 'f') {
                return;
            }
            ++position_;
        }
    }

    std::string text_;
    size_t position_ = 0;
};

static void loadScene() {
    std::ifstream input(gApp.scenePath);
    if (!input) {
        gApp.status = "Failed to open " + std::string(gApp.scenePath);
        return;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();

    JsonCursor root(text);
    Camera camera;
    if (root.findKey("camera")) {
        const std::string cameraBlock = root.readObjectBlock();
        JsonCursor cameraJson(cameraBlock);
        if (cameraJson.findKey("yaw")) {
            cameraJson.readFloat(camera.yaw);
        }
        if (cameraJson.findKey("pitch")) {
            cameraJson.readFloat(camera.pitch);
        }
        if (cameraJson.findKey("distance")) {
            cameraJson.readFloat(camera.distance);
        }
        if (cameraJson.findKey("target")) {
            cameraJson.readVec3(camera.target);
        }
    }

    JsonCursor rootLight(text);
    Light light;
    if (rootLight.findKey("light")) {
        const std::string lightBlock = rootLight.readObjectBlock();
        JsonCursor lightJson(lightBlock);
        if (lightJson.findKey("direction")) {
            lightJson.readVec3(light.direction);
        }
        if (lightJson.findKey("color")) {
            lightJson.readColor(light.color);
        }
        if (lightJson.findKey("intensity")) {
            lightJson.readFloat(light.intensity);
        }
        if (lightJson.findKey("ambient")) {
            lightJson.readFloat(light.ambient);
        }
    }

    std::vector<SceneObject> loaded;
    JsonCursor objectsJson(text);
    if (objectsJson.findKey("objects")) {
        while (true) {
            const std::string block = objectsJson.readObjectBlock();
            if (block.empty()) {
                break;
            }

            JsonCursor objectJson(block);
            SceneObject object;
            std::string meshName = "Cube";
            if (objectJson.findKey("name")) {
                objectJson.readString(object.name);
            }
            if (objectJson.findKey("mesh")) {
                objectJson.readString(meshName);
            }
            object.meshType = meshTypeFromName(meshName);
            if (isPrimitiveMesh(object.meshType)) {
                object.mesh = gApp.meshes[meshIndex(object.meshType)];
            }
            if (object.name.empty()) {
                object.name = std::string(meshTypeName(object.meshType)) + " " + std::to_string(loaded.size() + 1);
            }
            if (objectJson.findKey("position")) {
                objectJson.readVec3(object.position);
            }
            if (objectJson.findKey("rotation")) {
                objectJson.readVec3(object.rotation);
            }
            if (objectJson.findKey("scale")) {
                objectJson.readVec3(object.scale);
            }
            if (objectJson.findKey("color")) {
                objectJson.readColor(object.material.baseColor);
            }
            if (objectJson.findKey("checkerTexture")) {
                objectJson.readBool(object.material.useCheckerTexture);
            }
            if (objectJson.findKey("imageTexture")) {
                objectJson.readBool(object.material.useImageTexture);
            }
            if (objectJson.findKey("texturePath")) {
                objectJson.readString(object.material.texturePath);
                if (!object.material.texturePath.empty()) {
                    loadBmpTexture(object.material.texturePath, object.material.textureId);
                }
            }
            std::vector<Vec3> savedVertices;
            if (objectJson.findKey("vertices") && objectJson.readVec3Array(savedVertices) && (object.meshType == MeshType::Imported || savedVertices.size() == object.mesh.vertices.size())) {
                if (object.meshType == MeshType::Imported) {
                    object.mesh.vertices.clear();
                    for (const Vec3& position : savedVertices) {
                        object.mesh.vertices.push_back({position, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
                    }
                }
                for (size_t vertexIndex = 0; vertexIndex < savedVertices.size(); ++vertexIndex) {
                    object.mesh.vertices[vertexIndex].position = savedVertices[vertexIndex];
                }
            }
            std::vector<unsigned int> savedIndices;
            if (objectJson.findKey("indices") && objectJson.readUIntArray(savedIndices) && !savedIndices.empty()) {
                object.mesh.indices = savedIndices;
            }
            if (!object.mesh.vertices.empty() && !object.mesh.indices.empty()) {
                recalculateMesh(object.mesh);
            }
            loaded.push_back(object);
        }
    }

    if (loaded.empty()) {
        gApp.status = "No objects loaded from " + std::string(gApp.scenePath);
        return;
    }

    gApp.camera = camera;
    gApp.light = light;
    gApp.objects = loaded;
    gApp.selectedIndex = 0;
    clearEditSelection();
    gApp.status = "Loaded " + std::string(gApp.scenePath);
}

static bool intersectLocalBounds(Vec3 rayOrigin, Vec3 rayDirection, Vec3 boundsMin, Vec3 boundsMax, float& hitDistance) {
    float tMin = -std::numeric_limits<float>::infinity();
    float tMax = std::numeric_limits<float>::infinity();
    const float origin[3] = {rayOrigin.x, rayOrigin.y, rayOrigin.z};
    const float direction[3] = {rayDirection.x, rayDirection.y, rayDirection.z};
    const float minValue[3] = {boundsMin.x, boundsMin.y, boundsMin.z};
    const float maxValue[3] = {boundsMax.x, boundsMax.y, boundsMax.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 0.00001f) {
            if (origin[axis] < minValue[axis] || origin[axis] > maxValue[axis]) {
                return false;
            }
            continue;
        }

        float nearHit = (minValue[axis] - origin[axis]) / direction[axis];
        float farHit = (maxValue[axis] - origin[axis]) / direction[axis];
        if (nearHit > farHit) {
            std::swap(nearHit, farHit);
        }

        tMin = std::max(tMin, nearHit);
        tMax = std::min(tMax, farHit);
        if (tMin > tMax) {
            return false;
        }
    }

    hitDistance = tMin >= 0.0f ? tMin : tMax;
    return hitDistance >= 0.0f;
}

static Vec3 toObjectLocalPoint(Vec3 worldPoint, const SceneObject& object) {
    Vec3 local = worldPoint - object.position;
    local = rotateZ(local, -object.rotation.z);
    local = rotateY(local, -object.rotation.y);
    local = rotateX(local, -object.rotation.x);
    local.x /= std::max(0.0001f, object.scale.x);
    local.y /= std::max(0.0001f, object.scale.y);
    local.z /= std::max(0.0001f, object.scale.z);
    return local;
}

static Vec3 toObjectLocalDirection(Vec3 worldDirection, const SceneObject& object) {
    Vec3 local = rotateZ(worldDirection, -object.rotation.z);
    local = rotateY(local, -object.rotation.y);
    local = rotateX(local, -object.rotation.x);
    local.x /= std::max(0.0001f, object.scale.x);
    local.y /= std::max(0.0001f, object.scale.y);
    local.z /= std::max(0.0001f, object.scale.z);
    return normalize(local);
}

static bool intersectTriangle(Vec3 origin, Vec3 direction, Vec3 a, Vec3 b, Vec3 c, float& t) {
    const Vec3 edge1 = b - a;
    const Vec3 edge2 = c - a;
    const Vec3 p = cross(direction, edge2);
    const float determinant = dot(edge1, p);
    if (std::abs(determinant) < 0.000001f) {
        return false;
    }

    const float invDet = 1.0f / determinant;
    const Vec3 s = origin - a;
    const float u = invDet * dot(s, p);
    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    const Vec3 q = cross(s, edge1);
    const float v = invDet * dot(direction, q);
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    t = invDet * dot(edge2, q);
    return t >= 0.0f;
}

static float distanceRayToSegment(Vec3 rayOrigin, Vec3 rayDirection, Vec3 a, Vec3 b, float& rayT) {
    const Vec3 segment = b - a;
    const Vec3 w0 = rayOrigin - a;
    const float segLenSq = std::max(0.000001f, dot(segment, segment));
    const float bDot = dot(rayDirection, segment);
    const float dDot = dot(rayDirection, w0);
    const float eDot = dot(segment, w0);
    const float denom = segLenSq - bDot * bDot;

    float segmentT = 0.0f;
    if (std::abs(denom) > 0.000001f) {
        segmentT = std::clamp((bDot * dDot - eDot) / denom, 0.0f, 1.0f);
    }

    rayT = std::max(0.0f, segmentT * bDot - dDot);
    const Vec3 pointOnRay = rayOrigin + rayDirection * rayT;
    const Vec3 pointOnSegment = a + segment * segmentT;
    return length(pointOnRay - pointOnSegment);
}

static void pickEditElement(double mouseX, double mouseY, int width, int height) {
    if (gApp.objects.empty()) {
        return;
    }

    Vec3 eye;
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    cameraBasis(eye, forward, right, up);

    const float aspect = static_cast<float>(width) / static_cast<float>(std::max(height, 1));
    const float fovScale = std::tan(radians(55.0f) * 0.5f);
    const float ndcX = (2.0f * static_cast<float>(mouseX) / static_cast<float>(std::max(width, 1)) - 1.0f) * aspect * fovScale;
    const float ndcY = (1.0f - 2.0f * static_cast<float>(mouseY) / static_cast<float>(std::max(height, 1))) * fovScale;
    const Vec3 rayDirectionWorld = normalize(forward + right * ndcX + up * ndcY);

    SceneObject& object = gApp.objects[gApp.selectedIndex];
    Mesh& mesh = object.mesh;
    const Vec3 rayOrigin = toObjectLocalPoint(eye, object);
    const Vec3 rayDirection = toObjectLocalDirection(rayDirectionWorld, object);
    clearEditSelection();

    if (gApp.editSelectionMode == EditSelectionMode::Vertex) {
        int bestVertex = -1;
        float bestDistance = 0.06f;
        float bestT = std::numeric_limits<float>::infinity();
        for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
            const Vec3 toVertex = mesh.vertices[i].position - rayOrigin;
            const float t = dot(toVertex, rayDirection);
            if (t < 0.0f) {
                continue;
            }
            const float distance = length(rayOrigin + rayDirection * t - mesh.vertices[i].position);
            if (distance < bestDistance || (distance <= bestDistance && t < bestT)) {
                bestDistance = distance;
                bestT = t;
                bestVertex = i;
            }
        }
        gApp.selectedVertex = bestVertex;
        gApp.status = bestVertex >= 0 ? "Selected vertex " + std::to_string(bestVertex) : "No vertex under cursor";
        return;
    }

    if (gApp.editSelectionMode == EditSelectionMode::Edge) {
        int bestA = -1;
        int bestB = -1;
        float bestDistance = 0.045f;
        float bestT = std::numeric_limits<float>::infinity();
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const unsigned int edges[3][2] = {
                {mesh.indices[i], mesh.indices[i + 1]},
                {mesh.indices[i + 1], mesh.indices[i + 2]},
                {mesh.indices[i + 2], mesh.indices[i]}
            };
            for (const auto& edge : edges) {
                float rayT = 0.0f;
                const float distance = distanceRayToSegment(rayOrigin, rayDirection, mesh.vertices[edge[0]].position, mesh.vertices[edge[1]].position, rayT);
                if ((distance < bestDistance || (distance <= bestDistance && rayT < bestT)) && rayT >= 0.0f) {
                    bestDistance = distance;
                    bestT = rayT;
                    bestA = static_cast<int>(edge[0]);
                    bestB = static_cast<int>(edge[1]);
                }
            }
        }
        gApp.selectedEdgeA = bestA;
        gApp.selectedEdgeB = bestB;
        gApp.status = bestA >= 0 ? "Selected edge " + std::to_string(bestA) + "-" + std::to_string(bestB) : "No edge under cursor";
        return;
    }

    int bestTriangle = -1;
    float bestT = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const Vec3 a = mesh.vertices[mesh.indices[i]].position;
        const Vec3 b = mesh.vertices[mesh.indices[i + 1]].position;
        const Vec3 c = mesh.vertices[mesh.indices[i + 2]].position;
        float t = 0.0f;
        if (intersectTriangle(rayOrigin, rayDirection, a, b, c, t) && t < bestT) {
            bestT = t;
            bestTriangle = static_cast<int>(i / 3);
        }
    }
    gApp.selectedTriangle = bestTriangle;
    gApp.status = bestTriangle >= 0 ? "Selected face " + std::to_string(bestTriangle) : "No face under cursor";
}

static void pickObject(double mouseX, double mouseY, int width, int height) {
    Vec3 eye;
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    cameraBasis(eye, forward, right, up);

    const float aspect = static_cast<float>(width) / static_cast<float>(std::max(height, 1));
    const float fovScale = std::tan(radians(55.0f) * 0.5f);
    const float ndcX = (2.0f * static_cast<float>(mouseX) / static_cast<float>(std::max(width, 1)) - 1.0f) * aspect * fovScale;
    const float ndcY = (1.0f - 2.0f * static_cast<float>(mouseY) / static_cast<float>(std::max(height, 1))) * fovScale;
    const Vec3 rayDirection = normalize(forward + right * ndcX + up * ndcY);

    int bestIndex = -1;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (int i = 0; i < static_cast<int>(gApp.objects.size()); ++i) {
        const SceneObject& object = gApp.objects[i];
        const Mesh& mesh = object.mesh;
        const Vec3 localOrigin = toObjectLocalPoint(eye, object);
        const Vec3 localDirection = toObjectLocalDirection(rayDirection, object);
        float hitDistance = 0.0f;
        if (intersectLocalBounds(localOrigin, localDirection, mesh.boundsMin, mesh.boundsMax, hitDistance) && hitDistance < bestDistance) {
            bestDistance = hitDistance;
            bestIndex = i;
        }
    }

    if (bestIndex >= 0) {
        gApp.selectedIndex = bestIndex;
        clearEditSelection();
        gApp.status = "Selected " + gApp.objects[bestIndex].name;
    } else {
        gApp.status = "No object under cursor";
    }
}

static void createCheckerTexture() {
    constexpr int textureSize = 64;
    unsigned char pixels[textureSize * textureSize * 3] = {};
    for (int y = 0; y < textureSize; ++y) {
        for (int x = 0; x < textureSize; ++x) {
            const bool bright = ((x / 8) + (y / 8)) % 2 == 0;
            const int index = (y * textureSize + x) * 3;
            pixels[index + 0] = bright ? 235 : 80;
            pixels[index + 1] = bright ? 235 : 82;
            pixels[index + 2] = bright ? 225 : 88;
        }
    }

    glGenTextures(1, &gApp.checkerTexture);
    glBindTexture(GL_TEXTURE_2D, gApp.checkerTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSize, textureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void drawGrid(float size, int divisions) {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    const float half = size * 0.5f;
    const float step = size / static_cast<float>(divisions);

    glBegin(GL_LINES);
    for (int i = 0; i <= divisions; ++i) {
        const float value = -half + static_cast<float>(i) * step;
        const bool axis = std::abs(value) < 0.0001f;

        glColor3f(axis ? 0.18f : 0.28f, axis ? 0.48f : 0.30f, axis ? 0.85f : 0.34f);
        glVertex3f(-half, 0.0f, value);
        glVertex3f(half, 0.0f, value);

        glColor3f(axis ? 0.8f : 0.28f, axis ? 0.18f : 0.30f, axis ? 0.18f : 0.34f);
        glVertex3f(value, 0.0f, -half);
        glVertex3f(value, 0.0f, half);
    }
    glEnd();
}

static void drawAxes(float length = 2.0f) {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
    glColor3f(0.9f, 0.12f, 0.12f);
    glVertex3f(0.0f, 0.02f, 0.0f);
    glVertex3f(length, 0.02f, 0.0f);
    glColor3f(0.18f, 0.85f, 0.22f);
    glVertex3f(0.0f, 0.02f, 0.0f);
    glVertex3f(0.0f, length, 0.0f);
    glColor3f(0.16f, 0.45f, 1.0f);
    glVertex3f(0.0f, 0.02f, 0.0f);
    glVertex3f(0.0f, 0.02f, length);
    glEnd();

    glPointSize(8.0f);
    glBegin(GL_POINTS);
    glColor3f(0.9f, 0.12f, 0.12f);
    glVertex3f(length, 0.02f, 0.0f);
    glColor3f(0.18f, 0.85f, 0.22f);
    glVertex3f(0.0f, length, 0.0f);
    glColor3f(0.16f, 0.45f, 1.0f);
    glVertex3f(0.0f, 0.02f, length);
    glEnd();
    glPointSize(1.0f);
    glLineWidth(1.0f);
}

static void applyObjectTransform(const SceneObject& object) {
    glTranslatef(object.position.x, object.position.y, object.position.z);
    glRotatef(object.rotation.x, 1.0f, 0.0f, 0.0f);
    glRotatef(object.rotation.y, 0.0f, 1.0f, 0.0f);
    glRotatef(object.rotation.z, 0.0f, 0.0f, 1.0f);
    glScalef(object.scale.x, object.scale.y, object.scale.z);
}

static void drawMeshSurface(const Mesh& mesh, const SceneObject& object) {
    if (gApp.lightingEnabled) {
        glEnable(GL_LIGHTING);
    } else {
        glDisable(GL_LIGHTING);
    }

    if (object.material.useImageTexture && object.material.textureId != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, object.material.textureId);
    } else if (object.material.useCheckerTexture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, gApp.checkerTexture);
    } else {
        glDisable(GL_TEXTURE_2D);
    }

    const Color& color = object.material.baseColor;
    const float diffuse[] = {color.r, color.g, color.b, 1.0f};
    const float specular[] = {0.22f, 0.22f, 0.22f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
    glColor3f(color.r, color.g, color.b);

    glBegin(GL_TRIANGLES);
    for (unsigned int index : mesh.indices) {
        const Vertex& vertex = mesh.vertices[index];
        glNormal3f(vertex.normal.x, vertex.normal.y, vertex.normal.z);
        glTexCoord2f(vertex.uv.x * 4.0f, vertex.uv.y * 4.0f);
        glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
    }
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

static void drawMeshWireframe(const Mesh& mesh, bool selected) {
    if (!gApp.showWireframe && !selected) {
        return;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glColor3f(selected ? 1.0f : 0.06f, selected ? 0.92f : 0.06f, selected ? 0.25f : 0.06f);
    glLineWidth(selected ? 3.0f : 1.0f);

    glBegin(GL_LINES);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const Vertex& a = mesh.vertices[mesh.indices[i]];
        const Vertex& b = mesh.vertices[mesh.indices[i + 1]];
        const Vertex& c = mesh.vertices[mesh.indices[i + 2]];
        glVertex3f(a.position.x, a.position.y, a.position.z); glVertex3f(b.position.x, b.position.y, b.position.z);
        glVertex3f(b.position.x, b.position.y, b.position.z); glVertex3f(c.position.x, c.position.y, c.position.z);
        glVertex3f(c.position.x, c.position.y, c.position.z); glVertex3f(a.position.x, a.position.y, a.position.z);
    }
    glEnd();
    glLineWidth(1.0f);
}

static void drawEditOverlay(const Mesh& mesh) {
    if (!gApp.editMode) {
        return;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);

    glPointSize(7.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
        if (i == gApp.selectedVertex) {
            glColor3f(1.0f, 0.94f, 0.20f);
        } else {
            glColor3f(0.08f, 0.85f, 1.0f);
        }
        const Vec3& position = mesh.vertices[i].position;
        glVertex3f(position.x, position.y, position.z);
    }
    glEnd();

    if (gApp.selectedEdgeA >= 0 && gApp.selectedEdgeB >= 0) {
        glLineWidth(5.0f);
        glColor3f(1.0f, 0.94f, 0.20f);
        glBegin(GL_LINES);
        const Vec3& a = mesh.vertices[gApp.selectedEdgeA].position;
        const Vec3& b = mesh.vertices[gApp.selectedEdgeB].position;
        glVertex3f(a.x, a.y, a.z);
        glVertex3f(b.x, b.y, b.z);
        glEnd();
        glLineWidth(1.0f);
    }

    if (gApp.selectedTriangle >= 0) {
        const size_t i = static_cast<size_t>(gApp.selectedTriangle) * 3;
        if (i + 2 < mesh.indices.size()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1.0f, 0.94f, 0.20f, 0.38f);
            glBegin(GL_TRIANGLES);
            const Vec3& a = mesh.vertices[mesh.indices[i]].position;
            const Vec3& b = mesh.vertices[mesh.indices[i + 1]].position;
            const Vec3& c = mesh.vertices[mesh.indices[i + 2]].position;
            glVertex3f(a.x, a.y, a.z);
            glVertex3f(b.x, b.y, b.z);
            glVertex3f(c.x, c.y, c.z);
            glEnd();
            glDisable(GL_BLEND);
        }
    }

    glEnable(GL_DEPTH_TEST);
    glPointSize(1.0f);
}

static void drawObject(const SceneObject& object, bool selected) {
    const Mesh& mesh = object.mesh;
    glPushMatrix();
    applyObjectTransform(object);
    drawMeshSurface(mesh, object);
    glPolygonOffset(-1.0f, -1.0f);
    drawMeshWireframe(mesh, selected);
    if (selected) {
        drawEditOverlay(mesh);
    }
    glPopMatrix();
}

static void setupCameraProjection(int width, int height) {
    const float aspect = static_cast<float>(width) / static_cast<float>(std::max(height, 1));
    const float nearPlane = 0.1f;
    const float farPlane = 100.0f;
    const float top = std::tan(radians(55.0f) * 0.5f) * nearPlane;
    const float right = top * aspect;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-right, right, -top, top, nearPlane, farPlane);

    Vec3 eye;
    Vec3 forward;
    Vec3 rightVector;
    Vec3 up;
    cameraBasis(eye, forward, rightVector, up);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const float view[16] = {
        rightVector.x, up.x, -forward.x, 0.0f,
        rightVector.y, up.y, -forward.y, 0.0f,
        rightVector.z, up.z, -forward.z, 0.0f,
        0.0f,         0.0f, 0.0f,       1.0f
    };

    glMultMatrixf(view);
    glTranslatef(-eye.x, -eye.y, -eye.z);
}

static void setupLighting() {
    if (!gApp.lightingEnabled) {
        glDisable(GL_LIGHTING);
        return;
    }

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);

    const Vec3 direction = normalize(gApp.light.direction);
    const float lightDirection[] = {-direction.x, -direction.y, -direction.z, 0.0f};
    const float diffuse[] = {
        gApp.light.color.r * gApp.light.intensity,
        gApp.light.color.g * gApp.light.intensity,
        gApp.light.color.b * gApp.light.intensity,
        1.0f
    };
    const float ambient[] = {gApp.light.ambient, gApp.light.ambient, gApp.light.ambient, 1.0f};

    glLightfv(GL_LIGHT0, GL_POSITION, lightDirection);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, diffuse);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
}

static void renderScene(int width, int height) {
    glViewport(0, 0, width, height);
    glClearColor(0.055f, 0.061f, 0.071f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    setupCameraProjection(width, height);
    setupLighting();

    if (gApp.showGrid) {
        drawGrid(10.0f, 20);
        drawAxes(2.0f);
        setupLighting();
    }

    for (int i = 0; i < static_cast<int>(gApp.objects.size()); ++i) {
        drawObject(gApp.objects[i], i == gApp.selectedIndex);
    }
}

static Vec3 faceNormal(const Mesh& mesh, int triangleIndex) {
    const size_t i = static_cast<size_t>(triangleIndex) * 3;
    if (i + 2 >= mesh.indices.size()) {
        return {0.0f, 1.0f, 0.0f};
    }
    const Vec3 a = mesh.vertices[mesh.indices[i]].position;
    const Vec3 b = mesh.vertices[mesh.indices[i + 1]].position;
    const Vec3 c = mesh.vertices[mesh.indices[i + 2]].position;
    return normalize(cross(b - a, c - a));
}

static void pushSelectedFace(SceneObject& object, float amount) {
    if (gApp.selectedTriangle < 0) {
        return;
    }

    Mesh& mesh = object.mesh;
    const size_t i = static_cast<size_t>(gApp.selectedTriangle) * 3;
    if (i + 2 >= mesh.indices.size()) {
        return;
    }

    const Vec3 offset = faceNormal(mesh, gApp.selectedTriangle) * amount;
    mesh.vertices[mesh.indices[i]].position += offset;
    mesh.vertices[mesh.indices[i + 1]].position += offset;
    mesh.vertices[mesh.indices[i + 2]].position += offset;
    recalculateMesh(mesh);
}

static void drawUi() {
    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 690.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("MiniModeler");

    if (ImGui::Button("Cube")) {
        addPrimitive(MeshType::Cube);
    }
    ImGui::SameLine();
    if (ImGui::Button("Plane")) {
        addPrimitive(MeshType::Plane);
    }
    ImGui::SameLine();
    if (ImGui::Button("Sphere")) {
        addPrimitive(MeshType::Sphere);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cylinder")) {
        addPrimitive(MeshType::Cylinder);
    }

    if (ImGui::Button("Delete")) {
        deleteSelectedObject();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Camera")) {
        resetCamera();
    }

    ImGui::Checkbox("Grid", &gApp.showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Wire", &gApp.showWireframe);
    ImGui::SameLine();
    ImGui::Checkbox("Lighting", &gApp.lightingEnabled);
    ImGui::Checkbox("ImGui Demo", &gApp.showDemoWindow);

    ImGui::SeparatorText("Import");
    ImGui::InputText("OBJ Path", gApp.importPath, sizeof(gApp.importPath));
    if (ImGui::Button("Import OBJ")) {
        importObj(gApp.importPath);
    }

    ImGui::SeparatorText("Mode");
    if (ImGui::Checkbox("Edit Mode", &gApp.editMode)) {
        clearEditSelection();
    }
    if (gApp.editMode) {
        const char* modes[] = {"Vertex", "Edge", "Face"};
        int currentMode = static_cast<int>(gApp.editSelectionMode);
        if (ImGui::Combo("Select", &currentMode, modes, 3)) {
            gApp.editSelectionMode = static_cast<EditSelectionMode>(currentMode);
            clearEditSelection();
        }
    }

    ImGui::SeparatorText("Scene File");
    ImGui::InputText("Path", gApp.scenePath, sizeof(gApp.scenePath));
    if (ImGui::Button("Save JSON")) {
        saveScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load JSON")) {
        loadScene();
    }
    ImGui::TextWrapped("%s", gApp.status.c_str());

    ImGui::SeparatorText("Objects");
    for (int i = 0; i < static_cast<int>(gApp.objects.size()); ++i) {
        const bool selected = i == gApp.selectedIndex;
        const std::string label = gApp.objects[i].name + " (" + meshTypeName(gApp.objects[i].meshType) + ")";
        if (ImGui::Selectable(label.c_str(), selected)) {
            gApp.selectedIndex = i;
        }
    }

    if (!gApp.objects.empty()) {
        SceneObject& object = gApp.objects[gApp.selectedIndex];
        ImGui::SeparatorText("Transform");
        ImGui::DragFloat3("Position", &object.position.x, 0.05f);
        ImGui::DragFloat3("Rotation", &object.rotation.x, 1.0f);
        ImGui::DragFloat3("Scale", &object.scale.x, 0.02f, 0.05f, 10.0f);

        ImGui::SeparatorText("Material");
        ImGui::ColorEdit3("Base Color", &object.material.baseColor.r);
        ImGui::Checkbox("Checker Texture", &object.material.useCheckerTexture);
        ImGui::InputText("BMP Texture", gApp.texturePath, sizeof(gApp.texturePath));
        if (ImGui::Button("Load BMP Texture")) {
            if (loadBmpTexture(gApp.texturePath, object.material.textureId)) {
                object.material.texturePath = gApp.texturePath;
                object.material.useImageTexture = true;
                object.material.useCheckerTexture = false;
            }
        }
        ImGui::Checkbox("Use Image Texture", &object.material.useImageTexture);

        if (gApp.editMode) {
            ImGui::SeparatorText("Edit Selection");
            ImGui::Text("Mesh vertices: %d", static_cast<int>(object.mesh.vertices.size()));
            ImGui::Text("Mesh faces: %d", static_cast<int>(object.mesh.indices.size() / 3));

            if (gApp.editSelectionMode == EditSelectionMode::Vertex) {
                ImGui::Text("Selected vertex: %d", gApp.selectedVertex);
                if (gApp.selectedVertex >= 0 && gApp.selectedVertex < static_cast<int>(object.mesh.vertices.size())) {
                    if (ImGui::DragFloat3("Vertex Position", &object.mesh.vertices[gApp.selectedVertex].position.x, 0.01f)) {
                        recalculateMesh(object.mesh);
                    }
                }
            } else if (gApp.editSelectionMode == EditSelectionMode::Edge) {
                ImGui::Text("Selected edge: %d - %d", gApp.selectedEdgeA, gApp.selectedEdgeB);
                if (gApp.selectedEdgeA >= 0 && gApp.selectedEdgeB >= 0) {
                    if (ImGui::DragFloat3("Edge A", &object.mesh.vertices[gApp.selectedEdgeA].position.x, 0.01f)) {
                        recalculateMesh(object.mesh);
                    }
                    if (ImGui::DragFloat3("Edge B", &object.mesh.vertices[gApp.selectedEdgeB].position.x, 0.01f)) {
                        recalculateMesh(object.mesh);
                    }
                }
            } else {
                ImGui::Text("Selected face: %d", gApp.selectedTriangle);
                if (gApp.selectedTriangle >= 0) {
                    if (ImGui::Button("Push +")) {
                        pushSelectedFace(object, 0.05f);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Push -")) {
                        pushSelectedFace(object, -0.05f);
                    }
                }
            }
        }
    }

    ImGui::SeparatorText("Light");
    ImGui::DragFloat3("Direction", &gApp.light.direction.x, 0.02f, -1.0f, 1.0f);
    ImGui::ColorEdit3("Light Color", &gApp.light.color.r);
    ImGui::SliderFloat("Intensity", &gApp.light.intensity, 0.0f, 2.0f);
    ImGui::SliderFloat("Ambient", &gApp.light.ambient, 0.0f, 1.0f);

    ImGui::SeparatorText("Camera");
    ImGui::SliderFloat("Yaw", &gApp.camera.yaw, -180.0f, 180.0f);
    ImGui::SliderFloat("Pitch", &gApp.camera.pitch, -85.0f, 85.0f);
    ImGui::SliderFloat("Distance", &gApp.camera.distance, 2.0f, 40.0f);

    ImGui::Text("Left drag: orbit");
    ImGui::Text(gApp.editMode ? "Left click: select mesh element" : "Left click: select object");
    ImGui::Text("Right drag: pan");
    ImGui::Text("Scroll: zoom");
    ImGui::End();

    if (gApp.showDemoWindow) {
        ImGui::ShowDemoWindow(&gApp.showDemoWindow);
    }
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    gApp.lastMouseX = x;
    gApp.lastMouseY = y;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            gApp.leftDragging = true;
            gApp.mouseDownX = x;
            gApp.mouseDownY = y;
        } else if (action == GLFW_RELEASE) {
            gApp.leftDragging = false;
            const double dx = x - gApp.mouseDownX;
            const double dy = y - gApp.mouseDownY;
            if (dx * dx + dy * dy < 25.0) {
                int width = 0;
                int height = 0;
                glfwGetWindowSize(window, &width, &height);
                if (gApp.editMode) {
                    pickEditElement(x, y, width, height);
                } else {
                    pickObject(x, y, width, height);
                }
            }
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        gApp.rightDragging = action == GLFW_PRESS;
    }
}

static void cursorPositionCallback(GLFWwindow* window, double x, double y) {
    (void)window;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        gApp.lastMouseX = x;
        gApp.lastMouseY = y;
        return;
    }

    const double dx = x - gApp.lastMouseX;
    const double dy = y - gApp.lastMouseY;
    gApp.lastMouseX = x;
    gApp.lastMouseY = y;

    if (gApp.leftDragging) {
        gApp.camera.yaw -= static_cast<float>(dx) * 0.35f;
        gApp.camera.pitch += static_cast<float>(dy) * 0.35f;
        gApp.camera.pitch = std::clamp(gApp.camera.pitch, -85.0f, 85.0f);
    }

    if (gApp.rightDragging) {
        Vec3 eye;
        Vec3 forward;
        Vec3 right;
        Vec3 up;
        cameraBasis(eye, forward, right, up);
        const float speed = gApp.camera.distance * 0.0015f;
        gApp.camera.target = gApp.camera.target - right * (static_cast<float>(dx) * speed);
        gApp.camera.target = gApp.camera.target + up * (static_cast<float>(dy) * speed);
    }
}

static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
    (void)window;
    (void)xOffset;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }

    gApp.camera.distance -= static_cast<float>(yOffset) * 0.6f;
    gApp.camera.distance = std::clamp(gApp.camera.distance, 2.0f, 40.0f);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;

    if (action != GLFW_PRESS) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return;
    }

    const bool commandOrControl = (mods & GLFW_MOD_SUPER) != 0 || (mods & GLFW_MOD_CONTROL) != 0;

    if (commandOrControl && key == GLFW_KEY_S) {
        saveScene();
    } else if (commandOrControl && key == GLFW_KEY_O) {
        loadScene();
    } else if (key == GLFW_KEY_E) {
        gApp.editMode = !gApp.editMode;
        clearEditSelection();
    } else if (key == GLFW_KEY_A) {
        addPrimitive(MeshType::Cube);
    } else if (key == GLFW_KEY_P) {
        addPrimitive(MeshType::Plane);
    } else if (key == GLFW_KEY_U) {
        addPrimitive(MeshType::Sphere);
    } else if (key == GLFW_KEY_C) {
        addPrimitive(MeshType::Cylinder);
    } else if (key == GLFW_KEY_DELETE || key == GLFW_KEY_BACKSPACE) {
        deleteSelectedObject();
    } else if (key == GLFW_KEY_R) {
        resetCamera();
    }
}

int main() {
    buildMeshes();
    addPrimitive(MeshType::Cube);
    addPrimitive(MeshType::Sphere);
    addPrimitive(MeshType::Plane);

    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW.\n");
        return 1;
    }

    GLFWwindow* window = glfwCreateWindow(1280, 800, "MiniModelerCross", nullptr, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "Failed to create GLFW window.\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    createCheckerTexture();

    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int displayWidth = 0;
        int displayHeight = 0;
        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);

        renderScene(displayWidth, displayHeight);

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawUi();
        ImGui::Render();

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (gApp.checkerTexture != 0) {
        glDeleteTextures(1, &gApp.checkerTexture);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
