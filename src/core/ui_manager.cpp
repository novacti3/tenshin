#include "ui_manager.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <pfd/portable-file-dialogs.h>

#include "log.hpp"
#include "resource_manager.hpp"
#include "../misc/utils.hpp"

#include <utility>

#define ARRAY_SIZE(x) sizeof(x)/sizeof(x[0]) 

static constexpr char* GLSL_VERSION = "#version 150"; 

void UIManager::Init(GLFWwindow* const window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    ImGui::StyleColorsDark();
    
    //_windowFlags |= ImGuiWindowFlags_NoCollapse;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(GLSL_VERSION);
}
void UIManager::DeInit()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UIManager::DrawUI()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    DrawMainMenuBar();
    if(_showRendererProperties)
        DrawRendererPropertiesWindow();
    if(_showShaderProperties)
        DrawShaderPropertiesWindow();
    
    #ifdef _DEBUG
    if(_showImGuiDemoWindow)
        ImGui::ShowDemoWindow();
    #endif

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

std::vector<std::string> UIManager::ShowFileDialog(const std::string &title, const std::vector<std::string> &filters, bool allowMultiSelect)
{
    return pfd::open_file(title, "", filters, allowMultiSelect).result();
}

#pragma region Menus
void UIManager::DrawMainMenuBar()
{
    ImGui::BeginMainMenuBar();

    if(ImGui::BeginMenu("File"))
    {
        if(ImGui::MenuItem("Open file...", "CTRL+O", false, false))
        {
            // Open a file dialogue to let the user load a mesh
            auto path = ShowFileDialog("Select mesh", {"All files", "*", "OBJ files", ".obj"});
            // TODO: Notify the ResourceManager to load the provided mesh
        }
        if(ImGui::MenuItem("Close file", "CTRL+N", false, false))
        {
            // Close the currently loaded mesh
            // TODO: Notify the ResourceManager to unload the currently used mesh
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Exit", "ESC", false, false))
        {
            // Quit program
            // TODO: Notify the app to end
        }

        ImGui::EndMenu();
    }
    
    if(ImGui::BeginMenu("Windows"))
    {
        ImGui::MenuItem("Renderer properties", "", &_showRendererProperties, true);
        ImGui::MenuItem("Shader properties", "", &_showShaderProperties, true);
        #ifdef _DEBUG
        ImGui::Separator();
        ImGui::MenuItem("ImGui demo", "", &_showImGuiDemoWindow, true);
        #endif
     
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void UIManager::DrawRendererPropertiesWindow()
{
    if(ImGui::Begin("Renderer properties", &_showRendererProperties, _windowFlags))
    {
        static RendererSettings &rendererSettings = Renderer::getInstance().settings;

        UIManager::DrawWidgetColor("Background color", (float*)(&(rendererSettings.bgColor)));
        
        static bool renderWireframe = false;
        UIManager::DrawWidgetCheckbox("Draw wireframe", &renderWireframe);
        rendererSettings.renderMode = renderWireframe ? RenderMode::WIREFRAME : RenderMode::TRIANGLES;
    }
    ImGui::End();
}
void UIManager::DrawShaderPropertiesWindow()
{
    if(ImGui::Begin("Shader properties", &_showShaderProperties, _windowFlags))
    {
        // NOTE: Putting all of this combo stuff into a DrawWidgetCombo func would be great
        static const LoadedShadersMap &loadedShaders = ResourceManager::getInstance().getLoadedShaders();

        static std::vector<std::string> loadedShaderNames;
        // Only update the vector if necesarry
        if(loadedShaderNames.size() != loadedShaders.size())
        {
            for (const auto &shader: loadedShaders)
            {
                // Only add the shader into the combo if it isn't present in the vector already
                auto it = std::find(loadedShaderNames.begin(), loadedShaderNames.end(), shader.first);
                if(it == loadedShaderNames.end())
                {
                    loadedShaderNames.push_back(shader.first);
                }
            }
        }
        static int currentShader = loadedShaderNames.size() - 1;

        ImGui::Text("Loaded shaders"); ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth()/2);
        // NOTE: Maybe put this in a DrawWidgetCombo func of sorts
        if(ImGui::BeginCombo("", loadedShaderNames[currentShader].c_str()))
        {
            for(int i = 0; i < loadedShaders.size(); i++)
            {
                const std::string &shaderName = loadedShaderNames[i];
                const bool isSelected = (currentShader == i);
                if(ImGui::Selectable(shaderName.c_str(), isSelected))
                {
                    currentShader = i;
                    Scene::getInstance().shader = const_cast<Shader*>(ResourceManager::getInstance().GetShader(shaderName));
                }
            }
            ImGui::EndCombo(); 
        }
        
        ImGui::SameLine();
        if(ImGui::Button("X"))
        {
            const std::string &currentShaderName = loadedShaderNames[currentShader];
            if(currentShaderName != "default")
            {
                Scene::getInstance().shader = nullptr;
                ResourceManager::getInstance().UnloadShader(currentShaderName);
                
                int defaultShaderIndex = FindIndexOfElement<std::string>(loadedShaderNames, "default");
                currentShader = defaultShaderIndex != -1 ? defaultShaderIndex : 0;
            }
        }
        
        ImGui::SameLine();
        if(ImGui::Button("..."))
        {
            std::vector<std::string> shaderPaths = ShowFileDialog("Select shader", {"All files", "*", "Shader files", "*.vs *.fs"}, true);
            if(!shaderPaths.empty())
            {
                static struct FilePathSplitter
                {
                    std::pair<std::string, std::string> operator()(const std::string &path)
                    {
                        std::vector<std::string> splitPath = SplitString(path, '/');
                        std::vector<std::string> fileNameWithExtension = SplitString(splitPath[splitPath.size() - 1], '.');
                        std::string fileName = fileNameWithExtension[0];
                        std::string extension = fileNameWithExtension[1];

                        return make_pair(fileName, extension);
                    }
                } ParseFileNameAndExtension;

                using ShaderSourcePair = std::pair<std::string, std::string>;
                std::vector<ShaderSourcePair> shaderSources;

                using FileNameAndExtensionPair = std::pair<std::string, std::string>;

                // FIXME: Implement a check that prevents the user to load different files other than .vs and .fs
                // because loading an image for example causes a crash and that's not very cool
                int i = 0;
                while(shaderPaths.size() > 0)
                {
                    std::string path = shaderPaths[i];
                    FileNameAndExtensionPair fileNameAndExtension = ParseFileNameAndExtension(path);
                    
                    std::string secondShaderPath = path;
                    secondShaderPath = path.substr(0, path.length() - 3);

                    ShaderSourcePair newShader;
                    if(fileNameAndExtension.second == "fs")
                    {
                        secondShaderPath += ".vs";
                        auto it = std::find(shaderPaths.begin(), shaderPaths.end(), secondShaderPath);

                        if(it != shaderPaths.end())
                        {
                            newShader = newShader = make_pair(secondShaderPath, path);
                        }
                        else
                            newShader = newShader = make_pair("", path);
                    }
                    else if(fileNameAndExtension.second == "vs")
                    {
                        secondShaderPath += ".fs";
                        auto it = std::find(shaderPaths.begin(), shaderPaths.end(), secondShaderPath);

                        if(it != shaderPaths.end())
                        {
                            newShader = newShader = make_pair(path, secondShaderPath);
                        }
                        else
                            newShader = newShader = make_pair(path, "");
                    }

                    if(std::find(shaderSources.begin(), shaderSources.end(), newShader) == shaderSources.end())
                    {
                        shaderSources.push_back(newShader);
                        
                        if(newShader.first != "")
                            shaderPaths.erase(std::find(shaderPaths.begin(), shaderPaths.end(), newShader.first));
                        
                        if(newShader.second != "")
                            shaderPaths.erase(std::find(shaderPaths.begin(), shaderPaths.end(), newShader.second));
                    }

                    i = i < shaderPaths.size() ? i++ : shaderPaths.size();
                }

                for(const ShaderSourcePair &shaderSource: shaderSources)
                {
                    if(shaderSource.first.empty() || shaderSource.second.empty())
                    {
                        FileNameAndExtensionPair fileNameAndExtension = !shaderSource.first.empty() ? ParseFileNameAndExtension(shaderSource.first) : ParseFileNameAndExtension(shaderSource.second);
                        Log::LogError("Cannot load shader '" + fileNameAndExtension.first + "', only " + fileNameAndExtension.second + " shader provided");
                        continue;
                    }

                    ResourceManager::getInstance().LoadShaderFromFiles(shaderSource.first, shaderSource.second);
                }
            }
        }

        ImGui::Separator();

        ImGui::Text("Shader uniforms:");
        if(Scene::getInstance().shader != nullptr)
        {
            // Shader uniforms display and editing
            for(ShaderUniform* const uniform: Scene::getInstance().shader->getUniforms())
            {
                switch(uniform->getType())
                {
                    case ShaderUniformType::INT:
                        DrawWidgetInt(uniform->getName().c_str(), (int*)uniform->value);
                    break;

                    case ShaderUniformType::UINT:
                        DrawWidgetUnsignedInt(uniform->getName().c_str(), (unsigned int*)uniform->value);
                    break;

                    case ShaderUniformType::FLOAT:
                        DrawWidgetFloat(uniform->getName().c_str(), (float*)uniform->value);
                    break;

                    case ShaderUniformType::BOOL:
                        DrawWidgetCheckbox(uniform->getName().c_str(), (bool*)uniform->value);
                    break;


                    case ShaderUniformType::VEC2:
                        DrawWidgetVec2(uniform->getName().c_str(), (float*)uniform->value);
                    break;

                    case ShaderUniformType::VEC3:
                        DrawWidgetVec3(uniform->getName().c_str(), (float*)uniform->value);
                    break;

                    case ShaderUniformType::VEC4:
                        // TODO: Some sort of differenciation between regular vec4 and color
                        DrawWidgetColor(uniform->getName().c_str(), (float*)uniform->value);
                    break;


                    case ShaderUniformType::MAT2:
                    break;

                    case ShaderUniformType::MAT3:
                    break;

                    case ShaderUniformType::MAT4:
                        // Draw a 4x4 InputFloat widget thing
                    break;


                    case ShaderUniformType::TEX2D:
                        Texture *newTex = DrawWidgetTex2D(uniform->getName().c_str(), (Texture*)uniform->value);
                        if(newTex != nullptr)
                        {
                            if(((Texture*)(uniform->value))->getID() == 0)
                            {
                                delete((Texture*)(uniform->value));
                            }
                            uniform->value = (void*)newTex;
                        }
                    break;
                }
            }
        }
    }
    ImGui::End();
}
#pragma endregion

#pragma region Widgets
#pragma region Base types
void UIManager::DrawWidgetInt(const char* const label, int* const value)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text(label); ImGui::SameLine();
    ImGui::DragInt("", value, 1);
}
void UIManager::DrawWidgetUnsignedInt(const char* const label, unsigned int* const value)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text(label); ImGui::SameLine();
    ImGui::DragInt("", (int*)value, 1, 0, UINT_MAX, "%i");
}
void UIManager::DrawWidgetFloat(const char* const label, float* const value)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text(label); ImGui::SameLine();
    ImGui::DragFloat("", value, 0.5f);
}
void UIManager::DrawWidgetCheckbox(const char* const label, bool* const value)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text(label); ImGui::SameLine();
    ImGui::Checkbox("", value);
}
#pragma endregion

#pragma region Vectors
void UIManager::DrawWidgetVec2(const char* const label, float* const value)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text(label); ImGui::SameLine();
    ImGui::DragFloat2("", value, 0.5f);
}
void UIManager::DrawWidgetVec3(const char* const label, float* const value)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text(label); ImGui::SameLine();
    ImGui::DragFloat3("", value, 0.5f);
}
void UIManager::DrawWidgetVec4(const char* const label, float* const value)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text(label); ImGui::SameLine();
    ImGui::DragFloat4("", value, 0.5f);
}
void UIManager::DrawWidgetColor(const char* const label, float* const value)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text(label); ImGui::SameLine();
    ImGui::ColorEdit4("", value);
}
#pragma endregion

Texture* UIManager::DrawWidgetTex2D(const char* const label, Texture* const value)
{
    auto &texturesInScene = Scene::getInstance().textures;
    auto &loadedTextures = ResourceManager::getInstance().getLoadedTextures();
    static const Texture &missingImgTex = *(ResourceManager::getInstance().GetTexture("ui_image_missing"));
    static ImVec2 imgSize(128.0f, 128.0f);

    ImGui::AlignTextToFramePadding();
    ImGui::Text(label); ImGui::SameLine();
    
    void *img = nullptr; 
    if(value->getID() != 0)
    {
        img = (void*)value->getID();
    }
    else
    { 
        img = (void*)missingImgTex.getID();
    }
    if(ImGui::ImageButton(img, imgSize))
    {
        std::string path = ShowFileDialog("Select texture", {"All files", "*", "JPEG", "*.jpg", "PNG", "*.png"})[0];
        if(!path.empty())
        {
            Texture* const newTex = ResourceManager::getInstance().LoadTextureFromFile(path);
            if(newTex != nullptr)
            {
                auto it = std::find(texturesInScene.begin(), texturesInScene.end(), newTex);
                if(it == texturesInScene.end())
                    texturesInScene.push_back(newTex);
                
                return newTex;
            }
        }
    }

    ImGui::SameLine();
    // NOTE: Just doing if(ImGui::Button) didn't work here for some reason so workaround it is
    ImGui::Button("X");
    if(ImGui::IsItemActivated())
    {
        if(value->getID() != 0 && value->getID() != missingImgTex.getID())
        {
            auto texIt = std::find(texturesInScene.begin(), texturesInScene.end(), value);
            if(texIt != texturesInScene.end())
                *texIt = nullptr;

            std::string texKey;
            for(const auto &entry: loadedTextures)
            {
                if(entry.second.get() == value)
                {
                    texKey = entry.first;
                    break;
                }
            }
            
            ResourceManager::getInstance().UnloadTexture(texKey);

            return const_cast<Texture*>(ResourceManager::getInstance().GetTexture("tex_missing"));
        }
    }

    // NOTE: A combo to pick already loaded textures like it is with the shaders

    return nullptr;
}

#pragma endregion